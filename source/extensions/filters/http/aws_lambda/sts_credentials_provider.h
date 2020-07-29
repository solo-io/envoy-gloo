#pragma once

#include "envoy/api/api.h"
#include "envoy/common/pure.h"
#include "envoy/common/time.h"
#include "extensions/common/aws/credentials_provider.h"
#include "extensions/filters/http/aws_lambda/sts_fetcher.h"

#include "api/envoy/config/filter/http/aws_lambda/v2/aws_lambda.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {

  
namespace {
  constexpr char AWS_ROLE_ARN[] = "AWS_ROLE_ARN";
  constexpr char AWS_WEB_IDENTITY_TOKEN_FILE[] = "AWS_WEB_IDENTITY_TOKEN_FILE";
  constexpr char AWS_ROLE_SESSION_NAME[] = "AWS_ROLE_SESSION_NAME";
  constexpr char AWS_STS_REGIONAL_ENDPOINTS[] = "AWS_STS_REGIONAL_ENDPOINTS";
}

class StsConstantValues {
public:
  const std::string RegionalEndpoint{"https://sts.{}.amazonaws.com."};
  const std::string GlobalEndpoint{"https://sts.amazonaws.com."};
};

using StsConstants = ConstSingleton<StsConstantValues>;

class StsCredentialsProvider;
using StsCredentialsProviderPtr = std::unique_ptr<StsCredentialsProvider>;

class StsCredentialsProvider {
public:
  virtual ~StsCredentialsProvider() = default;

  class Callbacks {
  public:
    virtual ~Callbacks() = default;

    /**
     * Called on successful request
     *
     * @param credential the credentials
     */
    virtual void onSuccess(const Extensions::Common::Aws::Credentials& credential) PURE;

    /**
     * Called on completion of request.
     *
     * @param status the status of the request.
     */
    virtual void onFailure(CredentialsFailureStatus status) PURE;
  };

  // Context object to hold data needed for verifier.
  class Context {
  public:
    virtual ~Context() = default;

    /**
     * Returns the request headers wrapped in this context.
     *
     * @return the request headers.
     */
    virtual const std::string& roleArn() const PURE;

    /**
     * Returns the request callback wrapped in this context.
     *
     * @returns the request callback.
     */
    virtual Callbacks* callbacks() const PURE;


    /**
     * Returns the request callback wrapped in this context.
     *
     * @returns the fetcher.
     */
    virtual StsFetcherPtr& fetcher() PURE;

    /**
     * Cancel any pending requests for this context.
     */
    virtual void cancel() PURE;
  };



  using ContextSharedPtr = std::shared_ptr<Context>;


  // Lookup credentials cache map. The cache only stores Jwks specified in the config.
  virtual void find(absl::optional<std::string> role_arn, ContextSharedPtr context) PURE;

  // Factory function to create an instance.
  static StsCredentialsProviderPtr
  create(const envoy::config::filter::http::aws_lambda::v2::AWSLambdaConfig_ServiceAccountCredentials& config, Api::Api& api);
};

using ContextSharedPtr = std::shared_ptr<StsCredentialsProvider::Context>;

class ContextFactory {
public:
  ContextFactory(Upstream::ClusterManager& cm, Api::Api& api) : cm_(cm), api_(api) {};

  virtual ~ContextFactory() = default;

  virtual ContextSharedPtr create(StsCredentialsProvider::Callbacks* callbacks);

private:
  Upstream::ClusterManager& cm_;
  Api::Api& api_;
};

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
