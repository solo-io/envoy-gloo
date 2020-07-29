#pragma once

#include "envoy/common/pure.h"
#include "envoy/api/api.h"
#include "envoy/config/core/v3/http_uri.pb.h"
#include "envoy/upstream/cluster_manager.h"
#include "extensions/common/aws/credentials_provider.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {

// typedef std::shared_ptr<const Envoy::Extensions::Common::Aws::Credentials>
//     CredentialsConstSharedPtr;

class StsFetcher;
using StsFetcherPtr = std::unique_ptr<StsFetcher>;
/**
 * StsFetcher interface can be used to retrieve STS credentials
 * An instance of this interface is designed to retrieve one set of credentials at a time.
 */
class StsFetcher {
public:
  class StsReceiver {
  public:
    enum class Failure {
      /* A network error occurred causing STS credentials retrieval failure. */
      Network,
      /* A failure occurred when trying to parse the retrieved STS credential data. */
      InvalidSts,
    };

    virtual ~StsReceiver() = default;
    /*
     * Successful retrieval callback.
     * of the returned JWKS object.
     * @param jwks the JWKS object retrieved.
     */
    virtual void onStsSuccess(const Envoy::Extensions::Common::Aws::Credentials&& credentials, const SystemTime& expiration) PURE;
    /*
     * Retrieval error callback.
     * * @param reason the failure reason.
     */
    virtual void onStsError(Failure reason) PURE;
  };

  virtual ~StsFetcher() = default;

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
   * @param parent_span the active span to create children under
   * @param receiver the receiver of the fetched JWKS or error.
   */
  virtual void fetch(const envoy::config::core::v3::HttpUri& uri,
                      const std::string& role_arn,
                      const std::string& web_token,
                      StsReceiver& receiver) PURE;

  /*
   * Factory method for creating a StsFetcher.
   * @param cm the cluster manager to use during Sts retrieval
   * @return a StsFetcher instance
   */
  static StsFetcherPtr create(Upstream::ClusterManager& cm, Api::Api& api);
};


} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
