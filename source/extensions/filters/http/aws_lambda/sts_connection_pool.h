#pragma once

#include "envoy/api/api.h"
#include "envoy/common/pure.h"
#include "envoy/common/time.h"

#include "common/common/regex.h"
#include "common/singleton/const_singleton.h"

#include "extensions/common/aws/credentials_provider.h"
#include "extensions/filters/http/aws_lambda/sts_fetcher.h"

#include "api/envoy/config/filter/http/aws_lambda/v2/aws_lambda.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {

namespace {
/*
 * AssumeRoleWithIdentity returns a set of temporary credentials with a minimum
 * lifespan of 15 minutes.
 * https://docs.aws.amazon.com/STS/latest/APIReference/API_AssumeRoleWithWebIdentity.html
 *
 * In order to ensure that credentials never expire, we default to 2/3.
 *
 * This in combination with the very generous grace period which makes sure the
 * tokens are refreshed if they have < 5 minutes left on their lifetime. Whether
 * that lifetime is our prescribed, or from the response itself.
 */
constexpr std::chrono::milliseconds REFRESH_STS_CREDS =
    std::chrono::minutes(10);

} // namespace

class StsResponseRegexValues {
public:
  StsResponseRegexValues() {

    // Initialize regex strings, should never fail
    regex_access_key =
        Regex::Utility::parseStdRegex("<AccessKeyId>(.*?)</AccessKeyId>");
    regex_secret_key = Regex::Utility::parseStdRegex(
        "<SecretAccessKey>(.*?)</SecretAccessKey>");
    regex_session_token =
        Regex::Utility::parseStdRegex("<SessionToken>(.*?)</SessionToken>");
    regex_expiration =
        Regex::Utility::parseStdRegex("<Expiration>(.*?)</Expiration>");
  };

  std::regex regex_access_key;

  std::regex regex_secret_key;

  std::regex regex_session_token;

  std::regex regex_expiration;
};

using StsResponseRegex = ConstSingleton<StsResponseRegexValues>;

class StsConnectionPool;
using StsConnectionPoolPtr = std::shared_ptr<StsConnectionPool>;

class StsConnectionPool {
public:
  virtual ~StsConnectionPool() = default;

  class Callbacks {
  public:
    virtual ~Callbacks() = default;

    /**
     * Called on successful request
     *
     * @param credential the credentials
     * @param role_arn the role_arn used to create these credentials
     */
    virtual void onSuccess(std::shared_ptr<const StsCredentials>,
                           std::string_view role_arn) PURE;
  };

  // Context object to hold data needed for verifier.
  class Context {
  public:
    virtual ~Context() = default;

    class Callbacks {
    public:
      virtual ~Callbacks() = default;

      /**
       * Called on successful request
       *
       * @param credential the credentials
       */
      virtual void onSuccess(
          std::shared_ptr<const Envoy::Extensions::Common::Aws::Credentials>)
          PURE;

      /**
       * Called on completion of request.
       *
       * @param status the status of the request.
       */
      virtual void onFailure(CredentialsFailureStatus status) PURE;
    };

    /**
     * Returns the request callback wrapped in this context.
     *
     * @returns the request callback.
     */
    virtual StsConnectionPool::Context::Callbacks *callbacks() const PURE;

    /**
     * Returns the request callback wrapped in this context.
     *
     * @returns the request callback.
     */
    virtual void cancel() PURE;
  };

  using ContextPtr = std::unique_ptr<Context>;

  virtual void init(const envoy::config::core::v3::HttpUri &uri,
                    const absl::string_view web_token) PURE;

  virtual Context *add(StsConnectionPool::Context::Callbacks *callback) PURE;

  virtual bool requestInFlight() PURE;

  static StsConnectionPoolPtr create(Api::Api &api,
                                     Event::Dispatcher &dispatcher,
                                     const absl::string_view role_arn,
                                     StsConnectionPool::Callbacks *callbacks,
                                     StsFetcherPtr fetcher);
};

using ContextPtr = std::unique_ptr<StsConnectionPool::Context>;

class StsConnectionPoolFactory;
using StsConnectionPoolFactoryPtr = std::unique_ptr<StsConnectionPoolFactory>;

class StsConnectionPoolFactory {
public:
  virtual ~StsConnectionPoolFactory() = default;

  virtual StsConnectionPoolPtr build(const absl::string_view role_arn,
                                     StsConnectionPool::Callbacks *callbacks,
                                     StsFetcherPtr fetcher) PURE;

  static StsConnectionPoolFactoryPtr create(Api::Api &api,
                                     Event::Dispatcher &dispatcher);
};

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
