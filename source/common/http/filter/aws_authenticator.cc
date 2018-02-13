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

#include "openssl/digest.h"
#include "openssl/hmac.h"
#include "openssl/sha.h"

namespace Envoy {
namespace Http {

const std::string AwsAuthenticator::ALGORITHM = "AWS4-HMAC-SHA256";

const std::string AwsAuthenticator::SERVICE = "lambda";

// TODO: move service to sign function
AwsAuthenticator::AwsAuthenticator(const std::string &access_key,
                                   const std::string &secret_key)
    : access_key_(access_key), first_key_("AWS4" + secret_key) {
  SHA256_Init(&body_sha_);
}

AwsAuthenticator::~AwsAuthenticator() {}

void AwsAuthenticator::updatePayloadHash(const Envoy::Buffer::Instance &data) {

  uint64_t num_slices = data.getRawSlices(nullptr, 0);
  Envoy::Buffer::RawSlice slices[num_slices];
  data.getRawSlices(slices, num_slices);
  for (Envoy::Buffer::RawSlice &slice : slices) {
    SHA256_Update(&body_sha_, slice.mem_, slice.len_);
  }
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

void AwsAuthenticator::sign(Envoy::Http::HeaderMap *request_headers,
                            std::list<Envoy::Http::LowerCaseString> &&headers,
                            const std::string &region) {
  // we can't use the date provider interface as this is not the date header,
  // plus the date format is different. use slow method now, optimize in the
  // future.
  static Envoy::Http::LowerCaseString dateheader =
      Envoy::Http::LowerCaseString("x-amz-date");
  headers.push_back(dateheader);
  headers.sort(lowercasecompare);

  auto now = std::chrono::system_clock::now();
  std::string RequestDateTime =
      Envoy::DateFormatter("%Y%m%dT%H%M%SZ").fromTime(now);
  request_headers->addReferenceKey(dateheader, RequestDateTime);

  std::stringstream canonicalHeaders;
  std::stringstream signedHeaders;

  for (auto header = headers.begin(), end = headers.end(); header != end;
       header++) {
    const Envoy::Http::HeaderEntry *headerEntry = request_headers->get(*header);
    if (headerEntry == nullptr) {
      headerEntry = getMaybeInlineHeader(request_headers, *header);
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

  uint8_t payload_out[SHA256_DIGEST_LENGTH];
  SHA256_Final(payload_out, &body_sha_);
  std::string hexpayload =
      Envoy::Hex::encode(payload_out, SHA256_DIGEST_LENGTH);

  std::string HTTPRequestMethod = Envoy::Http::Headers::get().MethodValues.Post;
  const Envoy::Http::HeaderString &CanonicalURI =
      request_headers->Path()->value();
  auto CanonicalURILen = CanonicalURI.size();

  std::string CanonicalQueryString = Envoy::EMPTY_STRING;

  const char *query_string_start = Utility::findQueryStringStart(CanonicalURI);
  if (query_string_start != nullptr) {
    CanonicalURILen = query_string_start - CanonicalURI.c_str();

    // +1 to skip the question mark
    // These should be sorted alphabetically, but I will leave that to the
    // caller (which is internal, hence it's ok)
    CanonicalQueryString = query_string_start + 1;
  }

  SHA256_CTX cononicalRequestHash;
  SHA256_Init(&cononicalRequestHash);
  SHA256_Update(&cononicalRequestHash, HTTPRequestMethod.c_str(),
                HTTPRequestMethod.size());
  SHA256_Update(&cononicalRequestHash, "\n", 1);
  SHA256_Update(&cononicalRequestHash, CanonicalURI.c_str(), CanonicalURILen);
  SHA256_Update(&cononicalRequestHash, "\n", 1);
  SHA256_Update(&cononicalRequestHash, CanonicalQueryString.c_str(),
                CanonicalQueryString.size());
  SHA256_Update(&cononicalRequestHash, "\n", 1);
  SHA256_Update(&cononicalRequestHash, CanonicalHeaders.c_str(),
                CanonicalHeaders.size());
  SHA256_Update(&cononicalRequestHash, "\n", 1);
  SHA256_Update(&cononicalRequestHash, SignedHeaders.c_str(),
                SignedHeaders.size());
  SHA256_Update(&cononicalRequestHash, "\n", 1);
  SHA256_Update(&cononicalRequestHash, hexpayload.c_str(), hexpayload.size());

  uint8_t cononicalRequestHashOut[SHA256_DIGEST_LENGTH];
  SHA256_Final(cononicalRequestHashOut, &cononicalRequestHash);

  //  SHA256(static_cast<const uint8_t*>(static_cast<const
  //  void*>(CanonicalRequest.c_str())), CanonicalRequest.size(), out);
  std::string hashedCanonicalRequest =
      Envoy::Hex::encode(cononicalRequestHashOut, SHA256_DIGEST_LENGTH);

  std::string CredentialScopeDate =
      Envoy::DateFormatter("%Y%m%d").fromTime(now);

  std::stringstream credentialScopeStream;
  credentialScopeStream << CredentialScopeDate << "/" << region << "/"
                        << SERVICE << "/aws4_request";
  std::string CredentialScope = credentialScopeStream.str();

  /*
  auto kDate = doHMAC(tovec("AWS4" + kSecret), tovec(CredentialScopeDate));
  auto kRegion = doHMAC(kDate, tovec(Region));
  auto kService = doHMAC(kRegion, tovec(Service));
  auto kSigning = doHMAC(kService, tovec("aws4_request"));
*/
  auto evp = EVP_sha256();
  unsigned int out_len = EVP_MD_size(evp);
  uint8_t out[out_len];

  HMAC_CTX ctx;
  HMAC_CTX_init(&ctx);

  HMAC_Init_ex(&ctx, first_key_.data(), first_key_.size(), evp, nullptr);
  HMAC_Update(&ctx,
              reinterpret_cast<const uint8_t *>(CredentialScopeDate.c_str()),
              CredentialScopeDate.size());
  HMAC_Final(&ctx, out, &out_len);

  HMAC_Init_ex(&ctx, out, out_len, nullptr, nullptr);
  HMAC_Update(&ctx, reinterpret_cast<const uint8_t *>(region.c_str()),
              region.size());
  HMAC_Final(&ctx, out, &out_len);

  HMAC_Init_ex(&ctx, out, out_len, nullptr, nullptr);
  HMAC_Update(&ctx, reinterpret_cast<const uint8_t *>(SERVICE.c_str()),
              SERVICE.size());
  HMAC_Final(&ctx, out, &out_len);

  static std::string aws_request = "aws4_request";
  HMAC_Init_ex(&ctx, out, out_len, nullptr, nullptr);
  HMAC_Update(&ctx, reinterpret_cast<const uint8_t *>(aws_request.c_str()),
              aws_request.size());
  HMAC_Final(&ctx, out, &out_len);

  /*

  std::string StringToSign =
  Algorithm + '\n' +
  RequestDateTime + '\n' +
  CredentialScope + '\n' +
  hashedCanonicalRequest;
  */
  HMAC_Init_ex(&ctx, out, out_len, nullptr, nullptr);
  HMAC_Update(&ctx, reinterpret_cast<const uint8_t *>(ALGORITHM.c_str()),
              ALGORITHM.size());
  HMAC_Update(&ctx, reinterpret_cast<const uint8_t *>("\n"), 1);
  HMAC_Update(&ctx, reinterpret_cast<const uint8_t *>(RequestDateTime.c_str()),
              RequestDateTime.size());
  HMAC_Update(&ctx, reinterpret_cast<const uint8_t *>("\n"), 1);
  HMAC_Update(&ctx, reinterpret_cast<const uint8_t *>(CredentialScope.c_str()),
              CredentialScope.size());
  HMAC_Update(&ctx, reinterpret_cast<const uint8_t *>("\n"), 1);
  HMAC_Update(&ctx,
              reinterpret_cast<const uint8_t *>(hashedCanonicalRequest.c_str()),
              hashedCanonicalRequest.size());
  HMAC_Final(&ctx, out, &out_len);
  HMAC_CTX_cleanup(&ctx);

  std::string signature = Envoy::Hex::encode(out, out_len);

  std::stringstream authorizationvalue;
  authorizationvalue << ALGORITHM << " Credential=" << access_key_ << "/"
                     << CredentialScope << ", SignedHeaders=" << SignedHeaders
                     << ", Signature=" << signature;

  request_headers->insertAuthorization().value(authorizationvalue.str());
}

} // namespace Http
} // namespace Envoy
