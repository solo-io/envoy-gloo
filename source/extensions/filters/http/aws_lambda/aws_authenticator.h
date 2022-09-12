#pragma once
#include <set>
#include <string>

#include "envoy/buffer/buffer.h"
#include "envoy/common/time.h"
#include "envoy/http/header_map.h"

#include "source/common/singleton/const_singleton.h"

#include "openssl/digest.h"
#include "openssl/hmac.h"
#include "openssl/sha.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {

class AwsAuthenticatorValues {
public:
  const std::string Algorithm{"AWS4-HMAC-SHA256"};
  const std::string Service{"lambda"};
  const std::string Newline{"\n"};
  const Http::LowerCaseString DateHeader{"x-amz-date"};
  const Http::LowerCaseString SecurityTokenHeader{"x-amz-security-token"};
  const Http::LowerCaseString Host{"host"};
};

typedef ConstSingleton<AwsAuthenticatorValues> AwsAuthenticatorConsts;

typedef bool (*LowerCaseStringCompareFunc)(const Http::LowerCaseString &,
                                           const Http::LowerCaseString &);

typedef std::set<Http::LowerCaseString, LowerCaseStringCompareFunc> HeaderList;

class AwsAuthenticator {
public:
  AwsAuthenticator(TimeSource &time_source);
  AwsAuthenticator(TimeSource &time_source, const std::string *service);

  ~AwsAuthenticator();

  void init(const std::string *access_key, const std::string *secret_key,
            const std::string *session_token);

  void updatePayloadHash(const Buffer::Instance &data);

  void sign(Http::RequestHeaderMap *request_headers,
            const HeaderList &headers_to_sign, const std::string &region);

  /**
   * This creates a a list of headers to sign to be used by sign.
   */
  static HeaderList
  createHeaderToSign(std::initializer_list<Http::LowerCaseString> headers);

  std::string getBodyHexSha();

private:
  // TODO(yuval-k) can I refactor our the friendliness?
  friend class AwsAuthenticatorTest;

  std::string signWithTime(Http::RequestHeaderMap *request_headers,
                           const HeaderList &headers_to_sign,
                           const std::string &region, SystemTime now);

  std::string addDate(SystemTime now);

  std::pair<std::string, std::string>
  prepareHeaders(const HeaderList &headers_to_sign);

  void fetchUrl();
  std::string computeCanonicalRequestHash(const std::string &request_method,
                                          const std::string &canonical_Headers,
                                          const std::string &signed_headers,
                                          const std::string &hexpayload);
  std::string getCredntialScopeDate(SystemTime now);
  std::string getCredntialScope(const std::string &region,
                                const std::string &datenow);

  std::string computeSignature(const std::string &region,
                               const std::string &credential_scope_date,
                               const std::string &credential_scope,
                               const std::string &request_date_time,
                               const std::string &hashed_canonical_request);

  static bool lowercasecompare(const Http::LowerCaseString &i,
                               const Http::LowerCaseString &j);

  class Sha256 {
  public:
    static const int LENGTH = SHA256_DIGEST_LENGTH;
    Sha256();
    void update(const Buffer::Instance &data);
    void update(const std::string &data);
    void update(const absl::string_view &data);

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
    size_t length() const;
    void init(const std::string &data);
    void init(const uint8_t *bytes, size_t size);
    void update(const std::string &data);
    void update(std::initializer_list<const std::string *> strings);
    void update(const uint8_t *bytes, size_t size);
    void finalize(uint8_t *out, unsigned int *out_len);

  private:
    HMAC_CTX context_;
    const EVP_MD *evp_;
    bool firstinit{true};
  };

  template <typename T>
  static void recusiveHmacHelper(HMACSha256 &hmac, uint8_t *out,
                                 unsigned int &out_len, const T &what) {
    hmac.init(out, out_len);
    hmac.update(what);
    hmac.finalize(out, &out_len);
  }

  Sha256 body_sha_;

  TimeSource &time_source_;
  const std::string *access_key_{};
  const std::string *session_token_{};
  std::string first_key_;
  const std::string *service_{};
  const std::string *method_{};
  absl::string_view query_string_{};
  absl::string_view url_base_{};

  Http::RequestHeaderMap *request_headers_{};
};

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
