#pragma once

#include <list>
#include <string>

#include "envoy/buffer/buffer.h"
#include "envoy/http/header_map.h"

#include "openssl/sha.h"

namespace Envoy {
namespace Http {

class AwsAuthenticator {
public:
  AwsAuthenticator(const std::string &access_key,
                   const std::string &secret_key);

  ~AwsAuthenticator();

  void updatePayloadHash(const Envoy::Buffer::Instance &data);

  void sign(Envoy::Http::HeaderMap *request_headers,
            std::list<Envoy::Http::LowerCaseString> &&headers_to_sign,
            const std::string &region);

private:
  //  void lambdafy();
  const Envoy::Http::HeaderEntry *
  getMaybeInlineHeader(Envoy::Http::HeaderMap *request_headers,
                       const Envoy::Http::LowerCaseString &im);

  static bool lowercasecompare(const Envoy::Http::LowerCaseString &i,
                               const Envoy::Http::LowerCaseString &j);

  const std::string& access_key_;
  const std::string first_key_;

  SHA256_CTX body_sha_;

  static const std::string ALGORITHM;
  static const std::string SERVICE;
};

} // namespace Http
} // namespace Envoy
