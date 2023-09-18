#pragma once

#include "envoy/api/api.h"
#include "envoy/common/pure.h"
#include "envoy/config/core/v3/http_uri.pb.h"
#include "envoy/upstream/cluster_manager.h"

#include "source/extensions/common/aws/credentials_provider.h"
#include "source/extensions/filters/http/aws_lambda/aws_authenticator.h"
#include "source/extensions/filters/http/aws_lambda/sts_status.h"
#include "source/extensions/filters/http/aws_lambda/sts_response_parser.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {

namespace {
  constexpr char StsFormatString[] =
    "Action=AssumeRoleWithWebIdentity&RoleArn={}&RoleSessionName={}&"
    "WebIdentityToken={}&Version=2011-06-15";

  constexpr char StsChainedFormatString[] =
    "Action=AssumeRole&RoleArn={}&RoleSessionName={}&"
    "Version=2011-06-15";

constexpr char ExpiredTokenError[] = "ExpiredTokenException";
constexpr char SignaturedoesNotMatchError[] = "SignatureDoesNotMatch";
constexpr char CredentialScopeMessage[] = "Credential should be scoped to a valid region.";
} // namespace

class StsCredentials : public Extensions::Common::Aws::Credentials {
public:
  StsCredentials(absl::string_view access_key_id,
                 absl::string_view secret_access_key,
                 absl::string_view session_token,
                 const SystemTime &expiration_time)
      : Extensions::Common::Aws::Credentials(access_key_id, secret_access_key,
                                             session_token),
        expiration_time_(expiration_time){};

  const SystemTime &expirationTime() const { return expiration_time_; };

private:
  const SystemTime expiration_time_;
};

class StsFetcher;
using StsFetcherPtr = std::unique_ptr<StsFetcher>;

using StsCredentialsSharedPtr = std::shared_ptr<StsCredentials>;
using StsCredentialsConstSharedPtr = std::shared_ptr<const StsCredentials>;

/**
 * StsFetcher interface can be used to retrieve STS credentials
 * An instance of this interface is designed to retrieve one set of credentials
 * at a time and is therefore scoped to a give role.
 * If provided with existing credentials it will use them to chain assume a role
 * A chained role assumption is not allowed to be for more than 1 hour.
 * So we for now do not change the default 1 hour assumption if this is chained.
 * https://docs.aws.amazon.com
 * /IAM/latest/UserGuide/id_roles_terms-and-concepts.html
 */
class StsFetcher {
public:
  virtual ~StsFetcher() = default;

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
   * @param web_token the token to authenticate with
   * @param success the cb called on successful role assumption
   * @param failure the cb called on failed role assumption
   */
  virtual void fetch(const envoy::config::core::v3::HttpUri &uri,
                     const std::string region,
                     const absl::string_view role_arn,
                     const absl::string_view web_token,
                     StsCredentialsConstSharedPtr creds,
                     StsFetcher::Callbacks *callbacks)  PURE;

  /*
   * Factory method for creating a StsFetcher.
   * @param cm the cluster manager to use during Sts retrieval
   * @param api the api instance
   * @return a StsFetcher instance
   */
  static StsFetcherPtr create(Upstream::ClusterManager &cm, Api::Api &api);
};

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
