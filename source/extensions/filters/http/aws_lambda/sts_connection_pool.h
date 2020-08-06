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

class StsResponseRegexValues {
public:
  StsResponseRegex() {

    // Initialize regex strings, should never fail
    regex_access_key =
        Regex::Utility::parseStdRegex("<AccessKeyId>(.*?)</AccessKeyId>");
    regex_secret_key =
        Regex::Utility::parseStdRegex("<SecretAccessKey>(.*?)</SecretAccessKey>");
    regex_session_token =
        Regex::Utility::parseStdRegex("<SessionToken>(.*?)</SessionToken>");
    regex_expiration =
        Regex::Utility::parseStdRegex("<Expiration>(.*?)</Expiration>");
  };

  const std::regex regex_access_key;

  const std::regex regex_secret_key;

  const std::regex regex_session_token;

  const std::regex regex_expiration;
}

using StsResponseRegex = ConstSingleton<StsResponseRegexValues>;

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
    virtual void onSuccess(std::shared_ptr<const StsCredentials>, std::string_view role_arn) PURE;
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
    virtual Callbacks *callbacks() const PURE;

    /**
     * Cancel any pending requests for this context.
     */
    virtual void cancel() PURE;
  };

  using ContextPtr = std::unique_ptr<Context>;

  virtual void init(const envoy::config::core::v3::HttpUri &uri,
             const absl::string_view web_token) PURE;
  
  virtual Context* add(StsCredentialsProvider::Callbacks *callback) PURE;
}

using StsConnectionPoolPtr = std::unique_ptr<StsConnectionPool>;


class StsConnectionPoolImpl: public StsFetcher::Callbacks {
  StsConnectionPoolImpl(Upstream::ClusterManager &cm, Api::Api &api
             const absl::string_view role_arn, StsConnectionPool::Callbacks *callbacks)

  void init(const envoy::config::core::v3::HttpUri &uri,
             const absl::string_view web_token) override; 

  Context* create(StsConnectionPool::Context::Callbacks *callbacks) override;

  void onSuccess(const absl::string_view body) override;

  void onFailure(CredentialsFailureStatus status) override;

private:
  StsFetcherPtr fetcher_;
  std::string role_arn_;
  StsConnectionPool::Callbacks *callbacks_;


  std::list<ContextImpl> connection_list_;
};

using ContextPtr = std::unique_ptr<StsCredentialsProvider::Context>;

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
