#include "common/http/filter/aws_authenticator.h"

#include <algorithm>
#include <list>
#include <string>
#include <vector>

#include "envoy/http/header_map.h"

#include "common/common/empty_string.h"
#include "common/common/hex.h"
#include "common/common/utility.h"
#include "common/http/headers.h"
#include "common/http/utility.h"

namespace Envoy {
namespace Http {

const std::string AwsAuthenticator::ALGORITHM = "AWS4-HMAC-SHA256";

const std::string AwsAuthenticator::SERVICE = "lambda";

AwsAuthenticator::AwsAuthenticator() {
  // TODO(yuval-k) hardcoded for now
  service_ = &SERVICE;
  method_ = &Envoy::Http::Headers::get().MethodValues.Post;
}

void AwsAuthenticator::init(const std::string *access_key,
                            const std::string *secret_key) {
  access_key_ = access_key;
  const std::string &secret_key_ref = *secret_key;
  first_key_ = "AWS4" + secret_key_ref;
}

AwsAuthenticator::~AwsAuthenticator() {}

void AwsAuthenticator::updatePayloadHash(const Envoy::Buffer::Instance &data) {
  body_sha_.update(data);
}

const HeaderEntry *
AwsAuthenticator::getMaybeInlineHeader(Envoy::Http::HeaderMap *request_headers,
                                       const Envoy::Http::LowerCaseString &im) {
#define Q(x) #x
#define QUOTE(x) Q(x)

#define CHECK_INLINE_HEADER(name)                                              \
  static Envoy::Http::LowerCaseString name##Str =                              \
      Envoy::Http::LowerCaseString(std::string(QUOTE(name)), true);            \
  if (im == name##Str) {                                                       \
    return request_headers->name();                                            \
  }

  ALL_INLINE_HEADERS(CHECK_INLINE_HEADER)

  return nullptr;
}

bool AwsAuthenticator::lowercasecompare(const Envoy::Http::LowerCaseString &i,
                                        const Envoy::Http::LowerCaseString &j) {
  return (i.get() < j.get());
}

std::string AwsAuthenticator::addDate(
    std::chrono::time_point<std::chrono::system_clock> now) {
  static Envoy::Http::LowerCaseString dateheader =
      Envoy::Http::LowerCaseString("x-amz-date");
  sign_headers_.push_back(dateheader);

  std::string RequestDateTime =
      Envoy::DateFormatter("%Y%m%dT%H%M%SZ").fromTime(now);
  request_headers_->addReferenceKey(dateheader, RequestDateTime);
  return RequestDateTime;
}

std::pair<std::string, std::string> AwsAuthenticator::prepareHeaders() {

  std::stringstream canonicalHeaders;
  std::stringstream signedHeaders;

  for (auto header = sign_headers_.begin(), end = sign_headers_.end();
       header != end; header++) {
    const Envoy::Http::HeaderEntry *headerEntry =
        request_headers_->get(*header);
    if (headerEntry == nullptr) {
      headerEntry = getMaybeInlineHeader(request_headers_, *header);
    }

    auto headerName = header->get();
    canonicalHeaders << headerName;
    signedHeaders << headerName;

    canonicalHeaders << ':';
    if (headerEntry != nullptr) {
      canonicalHeaders << headerEntry->value().c_str();
      // TODO: add warning if null
    }
    canonicalHeaders << '\n';
    std::list<Envoy::Http::LowerCaseString>::const_iterator next = header;
    next++;
    if (next != end) {
      signedHeaders << ";";
    }
  }
  std::string CanonicalHeaders = canonicalHeaders.str();
  std::string SignedHeaders = signedHeaders.str();
  
  std::pair<std::string, std::string>  pair = std::make_pair(std::move(CanonicalHeaders), std::move(SignedHeaders));
  return pair;
}

std::string AwsAuthenticator::getBodyHexSha() {

  uint8_t payload_out[SHA256_DIGEST_LENGTH];
  body_sha_.finalize(payload_out);
  std::string hexpayload =
      Envoy::Hex::encode(payload_out, SHA256_DIGEST_LENGTH);
  return hexpayload;
}

void AwsAuthenticator::fetchUrl() {

  const Envoy::Http::HeaderString &CanonicalURI =
      request_headers_->Path()->value();
  url_len_ = CanonicalURI.size();
  url_start_ = CanonicalURI.c_str();
  const char *query_string_start = Utility::findQueryStringStart(CanonicalURI);
  // bug in: findQueryStringStart - query_string_start will never be null as
  // implied in config_impl.cc, but rather it is the end iterator of the string.
  if (query_string_start != nullptr) {
    url_len_ = query_string_start - url_start_;
    if (url_len_ < CanonicalURI.size()) {
      // we now know that query_string_start != std::end

      // +1 to skip the question mark
      // These should be sorted alphabetically, but I will leave that to the
      // caller (which is internal, hence it's ok)
      query_string_len_ = CanonicalURI.size() - url_len_ - 1;
      query_string_start_ = query_string_start + 1;
    } else {
      query_string_start = nullptr;
    }
  }
}

std::string AwsAuthenticator::computeCanonicalRequestHash(
    const std::string &HTTPRequestMethod,const std::string &CanonicalHeaders, const std::string &SignedHeaders,
    const std::string &hexpayload) {

  // Do iternal classes for sha and hmac.
  Sha256 canonicalRequestHash;

  canonicalRequestHash.update(HTTPRequestMethod);
  canonicalRequestHash.update('\n');
  canonicalRequestHash.update(url_start_, url_len_);
  canonicalRequestHash.update('\n');
  if (query_string_start_ != nullptr) {
    canonicalRequestHash.update(query_string_start_, query_string_len_);
  }
  canonicalRequestHash.update('\n');
  canonicalRequestHash.update(CanonicalHeaders);
  canonicalRequestHash.update('\n');
  canonicalRequestHash.update(SignedHeaders);
  canonicalRequestHash.update('\n');
  canonicalRequestHash.update(hexpayload);

  uint8_t cononicalRequestHashOut[SHA256_DIGEST_LENGTH];

  canonicalRequestHash.finalize(cononicalRequestHashOut);
  return Envoy::Hex::encode(cononicalRequestHashOut, SHA256_DIGEST_LENGTH);
}

std::string AwsAuthenticator::getCredntialScopeDate(
    std::chrono::time_point<std::chrono::system_clock> now) {

  std::string CredentialScopeDate =
      Envoy::DateFormatter("%Y%m%d").fromTime(now);
  return CredentialScopeDate;
}

std::string 
AwsAuthenticator::getCredntialScope(const std::string &region,
                                    const std::string &CredentialScopeDate) {

  std::stringstream credentialScopeStream;
  credentialScopeStream << CredentialScopeDate << "/" << region << "/"
                        << (*service_) << "/aws4_request";
  return credentialScopeStream.str();
}

std::string AwsAuthenticator::computeSignature(
    const std::string &region, const std::string &CredentialScopeDate,
    const std::string &CredentialScope, const std::string &RequestDateTime,
    const std::string &hashedCanonicalRequest) {

  /*
  kDate = doHMAC(tovec("AWS4" + kSecret), tovec(CredentialScopeDate));
  kRegion = doHMAC(kDate, tovec(Region));
  kService = doHMAC(kRegion, tovec(Service));
  kSigning = doHMAC(kService, tovec("aws4_request"));
*/
  HMACSha256 sighmac;
  unsigned int out_len = sighmac.length();
  uint8_t out[out_len];

  sighmac.init(first_key_);
  sighmac.update(CredentialScopeDate);
  sighmac.finalize(out, &out_len);

  sighmac.init(out, out_len);
  sighmac.update(region);
  sighmac.finalize(out, &out_len);

  sighmac.init(out, out_len);
  sighmac.update(*service_);
  sighmac.finalize(out, &out_len);

  static std::string aws_request = "aws4_request";
  sighmac.init(out, out_len);
  sighmac.update(aws_request);
  sighmac.finalize(out, &out_len);

  sighmac.init(out, out_len);
  sighmac.update(ALGORITHM);
  sighmac.update('\n');
  sighmac.update(RequestDateTime);
  sighmac.update('\n');
  sighmac.update(CredentialScope);
  sighmac.update('\n');
  sighmac.update(hashedCanonicalRequest);
  sighmac.finalize(out, &out_len);

  return Envoy::Hex::encode(out, out_len);
}

void AwsAuthenticator::sign(Envoy::Http::HeaderMap *request_headers,
                            std::list<Envoy::Http::LowerCaseString> &&headers,
                            const std::string &region) {
                            
  // we can't use the date provider interface as this is not the date header,
  // plus the date format is different. use slow method now, optimize in the
  // future.
  auto now = std::chrono::system_clock::now();
    
  std::string sig = signWithTime(request_headers, std::move(headers), region, now);
  request_headers->insertAuthorization().value(sig);
}

std::string AwsAuthenticator::signWithTime(Envoy::Http::HeaderMap *request_headers,
                            std::list<Envoy::Http::LowerCaseString> &&headers,
                            const std::string &region,
    std::chrono::time_point<std::chrono::system_clock> now) {
  sign_headers_ = std::move(headers);
  request_headers_ = request_headers;

  std::string RequestDateTime = addDate(now);
  sign_headers_.sort(lowercasecompare);

  auto &&preparedHeaders = prepareHeaders();
  std::string CanonicalHeaders = std::move(preparedHeaders.first);
  std::string SignedHeaders = std::move(preparedHeaders.second);

  std::string hexpayload = getBodyHexSha();

  fetchUrl();

  std::string hashedCanonicalRequest =
      computeCanonicalRequestHash(*method_, CanonicalHeaders, SignedHeaders, hexpayload);
  std::string CredentialScopeDate = getCredntialScopeDate(now);
  std::string CredentialScope = getCredntialScope(region, CredentialScopeDate);

  std::string signature =
      computeSignature(region, CredentialScopeDate, CredentialScope,
                       RequestDateTime, hashedCanonicalRequest);

  std::stringstream authorizationvalue;
  RELEASE_ASSERT(access_key_);
  authorizationvalue << ALGORITHM << " Credential=" << (*access_key_) << "/"
                     << CredentialScope << ", SignedHeaders=" << SignedHeaders
                     << ", Signature=" << signature;
  return authorizationvalue.str();
}

AwsAuthenticator::Sha256::Sha256() { SHA256_Init(&context_); }

void AwsAuthenticator::Sha256::update(const Envoy::Buffer::Instance &data) {
  uint64_t num_slices = data.getRawSlices(nullptr, 0);
  Envoy::Buffer::RawSlice slices[num_slices];
  data.getRawSlices(slices, num_slices);
  for (Envoy::Buffer::RawSlice &slice : slices) {
    update(static_cast<const uint8_t *>(slice.mem_), slice.len_);
  }
}

void AwsAuthenticator::Sha256::update(const std::string &data) {
  update(data.c_str(), data.size());
}

void AwsAuthenticator::Sha256::update(const uint8_t *bytes, size_t size) {
  SHA256_Update(&context_, bytes, size);
}

void AwsAuthenticator::Sha256::update(const char *chars, size_t size) {
  update(reinterpret_cast<const uint8_t *>(chars), size);
}

void AwsAuthenticator::Sha256::update(char c) { update(&c, 1); }

void AwsAuthenticator::Sha256::finalize(uint8_t *out) {
  SHA256_Final(out, &context_);
}

AwsAuthenticator::HMACSha256::HMACSha256() : evp_(EVP_sha256()) {
  HMAC_CTX_init(&context_);
}

AwsAuthenticator::HMACSha256::~HMACSha256() { HMAC_CTX_cleanup(&context_); }

size_t AwsAuthenticator::HMACSha256::length() { return EVP_MD_size(evp_); }

void AwsAuthenticator::HMACSha256::init(const std::string &data) {
  init(reinterpret_cast<const uint8_t *>(data.data()), data.size());
}

void AwsAuthenticator::HMACSha256::init(const uint8_t *bytes, size_t size) {
  HMAC_Init_ex(&context_, bytes, size, firstinit ? evp_ : nullptr, nullptr);
  firstinit = false;
}

void AwsAuthenticator::HMACSha256::update(const std::string &data) {
  update(reinterpret_cast<const uint8_t *>(data.c_str()), data.size());
}

void AwsAuthenticator::HMACSha256::update(char c) { update(&c, 1); }

void AwsAuthenticator::HMACSha256::update(const char *chars, size_t size) {
  update(reinterpret_cast<const uint8_t *>(chars), size);
}
void AwsAuthenticator::HMACSha256::update(const uint8_t *bytes, size_t size) {
  HMAC_Update(&context_, bytes, size);
}

void AwsAuthenticator::HMACSha256::finalize(uint8_t *out,
                                            unsigned int *out_len) {
  HMAC_Final(&context_, out, out_len);
}

} // namespace Http
} // namespace Envoy
