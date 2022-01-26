#pragma once

#include "envoy/api/api.h"
#include "envoy/common/pure.h"
#include "envoy/common/time.h"

#include "source/common/common/regex.h"
#include "source/common/singleton/const_singleton.h"

#include "source/extensions/common/aws/credentials_provider.h"
#include "source/extensions/filters/http/aws_lambda/sts_fetcher.h"

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

class StsConnectionPool;
using StsConnectionPoolPtr = std::unique_ptr<StsConnectionPool>;

class StsConnectionPool :  public Logger::Loggable<Logger::Id::aws> {
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
     * @param chained_requests the list of arns that rely on this result
     */
    virtual void onResult(std::shared_ptr<const StsCredentials>,
       std::string role_arn, std::list<std::string> chained_requests) PURE;
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
     * Cancels the request if it is in flight
     */
    virtual void cancel() PURE;
  };

  using ContextPtr = std::unique_ptr<Context>;

  virtual void init(const envoy::config::core::v3::HttpUri &uri,
    const absl::string_view web_token, StsCredentialsConstSharedPtr creds) PURE;
  virtual void setInFlight() PURE;
  virtual Context *add(StsConnectionPool::Context::Callbacks *callback) PURE;

  virtual void addChained( std::string role_arn)PURE;

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
                                     StsFetcherPtr fetcher) const PURE;

  static StsConnectionPoolFactoryPtr create(Api::Api &api,
                                            Event::Dispatcher &dispatcher);
};

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
