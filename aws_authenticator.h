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
  AwsAuthenticator(const std::string &access_key, const std::string &secret_key,
                   std::string &&service);

  ~AwsAuthenticator();

  void update_payload_hash(const Envoy::Buffer::Instance &data);

  void sign(Envoy::Http::HeaderMap *request_headers,
            std::list<Envoy::Http::LowerCaseString> &&headers_to_sign,
            const std::string &region);

private:
  //  void lambdafy();
  const Envoy::Http::HeaderEntry *
  get_maybe_inline_header(Envoy::Http::HeaderMap *request_headers,
                          const Envoy::Http::LowerCaseString &im);

  static bool lowercasecompare(const Envoy::Http::LowerCaseString &i,
                               const Envoy::Http::LowerCaseString &j);

  const std::string access_key_;
  const std::string first_key_;
  std::string service_;
  std::string host_;

  SHA256_CTX body_sha_;

  static const std::string ALGORITHM;
};

} // namespace Http
} // namespace Envoy
