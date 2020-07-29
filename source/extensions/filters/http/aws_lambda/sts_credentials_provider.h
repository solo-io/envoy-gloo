#pragma once

#include "envoy/api/api.h"
#include "envoy/common/pure.h"
#include "envoy/common/time.h"
#include "extensions/common/aws/credentials_provider.h"
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

/**
 * Interface to access all configured Jwt rules and their cached Jwks objects.
 * It only caches Jwks specified in the config.
 * Its usage:
 *     auto jwks_cache = StsCredentialsProvider::create(Config);
 *
 *     // for a given jwt
 *     auto jwks_data = jwks_cache->findByIssuer(jwt->getIssuer());
 *     if (!jwks_data->areAudiencesAllowed(jwt->getAudiences())) reject;
 *
 *     if (jwks_data->getJwksObj() == nullptr || jwks_data->isExpired()) {
 *        // Fetch remote Jwks.
 *        jwks_data->setRemoteJwks(remote_jwks_str);
 *     }
 *
 *     verifyJwt(jwks_data->getJwksObj(), jwt);
 */

class StsCredentialsProvider {
public:
  virtual ~StsCredentialsProvider() = default;

  class Callbacks {
  public:
    virtual ~Callbacks() = default;

    /**
     * Called on completion of request.
     *
     * @param status the status of the request.
     */
    virtual void onComplete(const Extensions::Common::Aws::Credentials& credentials) PURE;
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
    virtual Http::HeaderMap& headers() const PURE;

    /**
     * Returns the request callback wrapped in this context.
     *
     * @returns the request callback.
     */
    virtual Callbacks* callback() const PURE;

    /**
     * Cancel any pending requests for this context.
     */
    virtual void cancel() PURE;
  };

  using ContextSharedPtr = std::shared_ptr<Context>;


  // Lookup credentials cache map. The cache only stores Jwks specified in the config.
  virtual ContextSharedPtr find(const std::string& arn) PURE;

  // Factory function to create an instance.
  static StsCredentialsProviderPtr
  create(const envoy::extensions::filters::http::jwt_authn::v3::JwtAuthentication& config,
         TimeSource& time_source, Api::Api& api);
};

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
