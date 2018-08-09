#include "extensions/filters/http/aws/aws_authenticator.h"

#include <algorithm>
#include <list>
#include <string>
#include <vector>

#include "envoy/http/header_map.h"

#include "common/common/assert.h"
#include "common/common/empty_string.h"
#include "common/common/hex.h"
#include "common/common/utility.h"
#include "common/http/headers.h"
#include "common/http/utility.h"

namespace Envoy {
namespace Http {

const std::string AwsAuthenticator::ALGORITHM = "AWS4-HMAC-SHA256";

const std::string AwsAuthenticator::SERVICE = "lambda";
const std::string AwsAuthenticator::NEWLINE = "\n";

AwsAuthenticator::AwsAuthenticator() {
  // TODO(yuval-k) hardcoded for now
  service_ = &SERVICE;
  method_ = &Headers::get().MethodValues.Post;
}

void AwsAuthenticator::init(const std::string *access_key,
                            const std::string *secret_key) {
  access_key_ = access_key;
  const std::string &secret_key_ref = *secret_key;
  first_key_ = "AWS4" + secret_key_ref;
}

AwsAuthenticator::~AwsAuthenticator() {}

void AwsAuthenticator::updatePayloadHash(const Buffer::Instance &data) {
  body_sha_.update(data);
}

bool AwsAuthenticator::lowercasecompare(const LowerCaseString &i,
                                        const LowerCaseString &j) {
  return (i.get() < j.get());
}

std::string AwsAuthenticator::addDate(
    std::chrono::time_point<std::chrono::system_clock> now) {
  static LowerCaseString dateheader = LowerCaseString("x-amz-date");
  sign_headers_.push_back(dateheader);

  std::string request_date_time = DateFormatter("%Y%m%dT%H%M%SZ").fromTime(now);
  request_headers_->addReferenceKey(dateheader, request_date_time);
  return request_date_time;
}

std::pair<std::string, std::string> AwsAuthenticator::prepareHeaders() {
  std::stringstream canonical_headers_stream;
  std::stringstream signed_headers_stream;

  for (auto header = sign_headers_.begin(), end = sign_headers_.end();
       header != end; header++) {
    const HeaderEntry *headerEntry = request_headers_->get(*header);
    if (headerEntry == nullptr) {
      request_headers_->lookup(*header, &headerEntry);
    }

    auto headerName = header->get();
    canonical_headers_stream << headerName;
    signed_headers_stream << headerName;

    canonical_headers_stream << ':';
    if (headerEntry != nullptr) {
      canonical_headers_stream << headerEntry->value().c_str();
      // TODO: add warning if null
    }
    canonical_headers_stream << '\n';
    std::list<LowerCaseString>::const_iterator next = header;
    next++;
    if (next != end) {
      signed_headers_stream << ";";
    }
  }
  std::string canonical_headers = canonical_headers_stream.str();
  std::string signed_headers = signed_headers_stream.str();

  std::pair<std::string, std::string> pair =
      std::make_pair(std::move(canonical_headers), std::move(signed_headers));
  return pair;
}

std::string AwsAuthenticator::getBodyHexSha() {

  uint8_t payload_out[SHA256_DIGEST_LENGTH];
  body_sha_.finalize(payload_out);
  std::string hexpayload = Hex::encode(payload_out, SHA256_DIGEST_LENGTH);
  return hexpayload;
}

void AwsAuthenticator::fetchUrl() {

  const HeaderString &canonical_url = request_headers_->Path()->value();
  url_len_ = canonical_url.size();
  url_start_ = canonical_url.c_str();
  const char *query_string_start = Utility::findQueryStringStart(canonical_url);
  // bug in: findQueryStringStart - query_string_start will never be null as
  // implied in config_impl.cc, but rather it is the end iterator of the string.
  if (query_string_start != nullptr) {
    url_len_ = query_string_start - url_start_;
    if (url_len_ < canonical_url.size()) {
      // we now know that query_string_start != std::end

      // +1 to skip the question mark
      // These should be sorted alphabetically, but I will leave that to the
      // caller (which is internal, hence it's ok)
      query_string_len_ = canonical_url.size() - url_len_ - 1;
      query_string_start_ = query_string_start + 1;
    } else {
      query_string_start = nullptr;
    }
  }
}

std::string AwsAuthenticator::computeCanonicalRequestHash(
    const std::string &request_method, const std::string &canonical_headers,
    const std::string &signed_headers, const std::string &hexpayload) {

  // Do iternal classes for sha and hmac.
  Sha256 canonicalRequestHash;

  canonicalRequestHash.update(request_method);
  canonicalRequestHash.update('\n');
  canonicalRequestHash.update(url_start_, url_len_);
  canonicalRequestHash.update('\n');
  if (query_string_start_ != nullptr) {
    canonicalRequestHash.update(query_string_start_, query_string_len_);
  }
  canonicalRequestHash.update('\n');
  canonicalRequestHash.update(canonical_headers);
  canonicalRequestHash.update('\n');
  canonicalRequestHash.update(signed_headers);
  canonicalRequestHash.update('\n');
  canonicalRequestHash.update(hexpayload);

  uint8_t cononicalRequestHashOut[SHA256_DIGEST_LENGTH];

  canonicalRequestHash.finalize(cononicalRequestHashOut);
  return Hex::encode(cononicalRequestHashOut, SHA256_DIGEST_LENGTH);
}

std::string AwsAuthenticator::getCredntialScopeDate(
    std::chrono::time_point<std::chrono::system_clock> now) {

  std::string credentials_scope_date = DateFormatter("%Y%m%d").fromTime(now);
  return credentials_scope_date;
}

std::string
AwsAuthenticator::getCredntialScope(const std::string &region,
                                    const std::string &credentials_scope_date) {

  std::stringstream credential_scope_stream;
  credential_scope_stream << credentials_scope_date << "/" << region << "/"
                          << (*service_) << "/aws4_request";
  return credential_scope_stream.str();
}

std::string AwsAuthenticator::computeSignature(
    const std::string &region, const std::string &credentials_scope_date,
    const std::string &credential_scope, const std::string &request_date_time,
    const std::string &hashed_canonical_request) {
  static std::string aws_request = "aws4_request";

  HMACSha256 sighmac;
  unsigned int out_len = sighmac.length();
  uint8_t out[out_len];

  sighmac.init(first_key_);
  sighmac.update(credentials_scope_date);
  sighmac.finalize(out, &out_len);

  recusiveHmacHelper(sighmac, out, out_len, region);
  recusiveHmacHelper(sighmac, out, out_len, *service_);
  recusiveHmacHelper(sighmac, out, out_len, aws_request);
  recusiveHmacHelper<std::initializer_list<const std::string *>>(
      sighmac, out, out_len,
      {&ALGORITHM, &NEWLINE, &request_date_time, &NEWLINE, &credential_scope,
       &NEWLINE, &hashed_canonical_request});

  return Hex::encode(out, out_len);
}

void AwsAuthenticator::sign(HeaderMap *request_headers,
                            std::list<LowerCaseString> &&headers,
                            const std::string &region) {

  // we can't use the date provider interface as this is not the date header,
  // plus the date format is different. use slow method now, optimize in the
  // future.
  auto now = std::chrono::system_clock::now();

  std::string sig =
      signWithTime(request_headers, std::move(headers), region, now);
  request_headers->insertAuthorization().value(sig);
}

std::string AwsAuthenticator::signWithTime(
    HeaderMap *request_headers, std::list<LowerCaseString> &&headers,
    const std::string &region,
    std::chrono::time_point<std::chrono::system_clock> now) {
  sign_headers_ = std::move(headers);
  request_headers_ = request_headers;

  std::string request_date_time = addDate(now);
  sign_headers_.sort(lowercasecompare);

  auto &&preparedHeaders = prepareHeaders();
  std::string canonical_headers = std::move(preparedHeaders.first);
  std::string signed_headers = std::move(preparedHeaders.second);

  std::string hexpayload = getBodyHexSha();

  fetchUrl();

  std::string hashed_canonical_request = computeCanonicalRequestHash(
      *method_, canonical_headers, signed_headers, hexpayload);
  std::string credentials_scope_date = getCredntialScopeDate(now);
  std::string CredentialScope =
      getCredntialScope(region, credentials_scope_date);

  std::string signature =
      computeSignature(region, credentials_scope_date, CredentialScope,
                       request_date_time, hashed_canonical_request);

  std::stringstream authorizationvalue;

  // TODO(talnordan): Provide `DETAILS`.
  RELEASE_ASSERT(access_key_, "");

  authorizationvalue << ALGORITHM << " Credential=" << (*access_key_) << "/"
                     << CredentialScope << ", SignedHeaders=" << signed_headers
                     << ", Signature=" << signature;
  return authorizationvalue.str();
}

AwsAuthenticator::Sha256::Sha256() { SHA256_Init(&context_); }

void AwsAuthenticator::Sha256::update(const Buffer::Instance &data) {
  uint64_t num_slices = data.getRawSlices(nullptr, 0);
  Buffer::RawSlice slices[num_slices];
  data.getRawSlices(slices, num_slices);
  for (Buffer::RawSlice &slice : slices) {
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

size_t AwsAuthenticator::HMACSha256::length() const {
  return EVP_MD_size(evp_);
}

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

void AwsAuthenticator::HMACSha256::update(
    std::initializer_list<const std::string *> strings) {
  for (auto &&str : strings) {
    update(*str);
  }
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
