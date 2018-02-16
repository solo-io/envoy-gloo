#pragma once

#include <list>
#include <string>

#include "envoy/buffer/buffer.h"
#include "envoy/http/header_map.h"

#include "openssl/digest.h"
#include "openssl/hmac.h"
#include "openssl/sha.h"

namespace Envoy {
namespace Http {

class AwsAuthenticator {
public:
  AwsAuthenticator();

  ~AwsAuthenticator();

  void init(const std::string *access_key, const std::string *secret_key);

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

  class Sha256 {
  public:
    static const int LENGTH = SHA256_DIGEST_LENGTH;
    Sha256();
    void update(const Envoy::Buffer::Instance &data);
    void update(const std::string &data);
    void update(char c);
    void update(const uint8_t *bytes, size_t size);
    void update(const char *chars, size_t size);
    void finalize(uint8_t *out);

  private:
    SHA256_CTX context_;
  };

  class HMACSha256 {
  public:
    HMACSha256();
    ~HMACSha256();
    size_t length();
    void init(const std::string &data);
    void init(const uint8_t *bytes, size_t size);
    void update(const std::string &data);
    void update(char c);
    void update(const char *chars, size_t size);
    void update(const uint8_t *bytes, size_t size);
    void finalize(uint8_t *out, unsigned int *out_len);

  private:
    HMAC_CTX context_;
    const EVP_MD *evp_;
  };

  Sha256 body_sha_;

  const std::string *access_key_;
  std::string first_key_;

  static const std::string ALGORITHM;
  static const std::string SERVICE;
};

} // namespace Http
} // namespace Envoy
