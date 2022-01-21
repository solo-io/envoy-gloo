#pragma once

#include "envoy/api/api.h"
#include "envoy/common/pure.h"
#include "envoy/config/core/v3/http_uri.pb.h"
#include "envoy/upstream/cluster_manager.h"


#include "source/extensions/filters/http/aws_lambda/sts_chained_fetcher.h"
#include "source/extensions/filters/http/aws_lambda/aws_authenticator.h"
#include "source/extensions/common/aws/credentials_provider.h"
#include "source/extensions/filters/http/aws_lambda/sts_status.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {

namespace {
constexpr char StsChainedFormatString[] =
    "Action=AssumeRole&RoleArn={}&RoleSessionName={}&"
    "Version=2011-06-15";

} // namespace


class StsChainedFetcher;
using StsChainedFetcherPtr = std::unique_ptr<StsChainedFetcher>;

/**
 * StsChainedFetcher interface can be used to retrieve STS credentials 
 * for a new account if you already have assumed a role.
 * An instance of this interface is designed to retrieve one set of credentialssess
 * at a time and is scoped to a given role.
 * Does not currently support multi-chaining.
 */
class StsChainedFetcher {
public:
  virtual ~StsChainedFetcher() = default;

  class ChainedCallback {
  public:
    virtual ~ChainedCallback() = default;

    /**
     * Called on successful request
     *
     * @param body the request body
     */
    virtual void onChainedSuccess(const std::string access_key, 
   const std::string secret_key, 
   const std::string session_token, 
   const std::string  expiration) PURE;

    /**
     * Called on completion of a failed request.
     *
     * @param status the status of the request.
     */
    virtual void onChainedFailure(CredentialsFailureStatus status) PURE;
  };

  class Callbacks {
    public:
      virtual ~Callbacks() = default;

      /**
      * Called on successful request
      *
      * @param body the request body
      */
      virtual void onSuccess(const absl::string_view body) PURE;

      /**
      * Called on completion of a failed request.
      *
      * @param status the status of the request.
      */
      virtual void onFailure(CredentialsFailureStatus status) PURE;
  };


    /*
    * Cancel any in-flight request.
    */
    virtual void cancel() PURE;

    /*
    * At most one outstanding request may be in-flight,
    * i.e. from the invocation of `fetch()` until either
    * a callback or `cancel()` is invoked, no
    * additional `fetch()` may be issued.
    * @param uri the uri to retrieve the jwks from.
    * @param role_arn the role_arn which of the role to assume
    * @param success the cb called on successful role assumption
    * @param failure the cb called on failed role assumption
    */
    virtual void fetch(const envoy::config::core::v3::HttpUri &uri,
                      const absl::string_view role_arn,
                        const std::string access_key, 
                      const std::string secret_key,
                      const std::string session_token,
                      StsChainedFetcher::ChainedCallback *callback)  PURE;

    /*
    * Factory method for creating a StsChainedFetcher.
    * @param cm the cluster manager to use during Sts retrieval
    * @param api the api instance
    * @return a StsChainedFetcher instance
    */
    static StsChainedFetcherPtr create(Upstream::ClusterManager &cm, Api::Api &api,
                                          const absl::string_view base_role_arn );
  };
 
} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
