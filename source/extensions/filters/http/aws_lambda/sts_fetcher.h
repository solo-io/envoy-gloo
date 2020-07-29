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



class StsCredentials : public Extensions::Common::Aws::Credentials {
public:
  StsCredentials(absl::string_view access_key_id,
                absl::string_view secret_access_key,
                absl::string_view session_token,
                const SystemTime& expiration_time) : Extensions::Common::Aws::Credentials(access_key_id, secret_access_key, session_token), expiration_time_(expiration_time) {};

  const SystemTime& expirationTime() const {return expiration_time_;};

private:
  const SystemTime expiration_time_;
};

class StsFetcher;
using StsFetcherPtr = std::unique_ptr<StsFetcher>;

using StsCredentialsSharedPtr = std::shared_ptr<StsCredentials>;
using StsCredentialsConstSharedPtr = std::shared_ptr<const StsCredentials>;

/**
 * StsFetcher interface can be used to retrieve STS credentials
 * An instance of this interface is designed to retrieve one set of credentials at a time.
 */
class StsFetcher {
public:

  virtual ~StsFetcher() = default;

  enum class Failure {
    /* A network error occurred causing STS credentials retrieval failure. */
    Network,
    /* A failure occurred when trying to parse the retrieved STS credential data. */
    InvalidSts,
  };

  using SuccessCallback = std::function<void(StsCredentialsConstSharedPtr& sts_credentials)>;

  using FailureCallback = std::function<void(Failure reason)>;

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
                      SuccessCallback success, FailureCallback failure) PURE;

  /*
   * Factory method for creating a StsFetcher.
   * @param cm the cluster manager to use during Sts retrieval
   * @return a StsFetcher instance
   */
  static StsFetcherPtr create(Upstream::ClusterManager& cm, Api::Api& api);
};

class StsFetcherFactory {
public:
  StsFetcherFactory(Upstream::ClusterManager& cm, Api::Api& api) : cm_(cm), api_(api) {};

  virtual ~StsFetcherFactory() = default;

  StsFetcherPtr create();

private:
  Upstream::ClusterManager& cm_;
  Api::Api& api_;
};

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
