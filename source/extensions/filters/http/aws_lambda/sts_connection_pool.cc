#include "extensions/filters/http/aws_lambda/sts_connection_pool.h"

#include "envoy/api/api.h"
#include "envoy/common/pure.h"
#include "envoy/common/time.h"
#include "common/common/linked_object.h"

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

constexpr std::chrono::minutes REFRESH_GRACE_PERIOD{5};

} // namespace

class ContextImpl : public StsCredentialsProvider::Context, 
                    public Event::DeferredDeletable,
                    public Envoy::LinkedObject<ContextImpl> {
public:
  ContextImpl(StsConnectionPool::Context::Callbacks *callbacks)
      : callbacks_(callbacks) {}

  StsCredentialsProvider::Callbacks *callbacks() const override {
    return callbacks_;
  }

private:
  StsConnectionPool::Context::Callbacks *callbacks_;
};

StsConnectionPoolImpl::StsConnectionPoolImpl(Upstream::ClusterManager &cm, Api::Api &api, const absl::string_view web_token, StsConnectionPool::Callbacks *callbacks): 
  fetcher_(StsFetcher::create(cm, api)), web_token_(web_token), callbacks_(callbacks) {};

StsConnectionPoolImpl::~StsConnectionPoolImpl() {
  for (auto&& ctx : connection_list_) {
    ctx.callbacks()->onFailure(CredentialsFailureStatus::ContextCancelled)
  }
  if (fetcher_ != nullptr) {
    fetcher_->cancel();
  }
};

void StsConnectionPoolImpl::init(
            const envoy::config::core::v3::HttpUri &uri,
            const absl::string_view web_token) {
  fetcher_.fetch(
    uri, 
    role_arn_, 
    web_token,
    *this,
  );
}

Context* StsConnectionPoolImpl::add(StsCredentialsProvider::Callbacks *callbacks) {
  LinkedList::moveIntoList(ctx, connection_list_);
};

void StsConnectionPoolImpl::onSuccess(const absl::string_view body) {
  ASSERT(body != nullptr);

// using a macro as we need to return on error
// TODO(yuval-k): we can use string_view instead of string when we upgrade to
// newer absl.
#define GET_PARAM(X)                                                           \
  std::string X;                                                               \
  {                                                                            \
    std::match_results<absl::string_view::const_iterator> matched;             \
    bool result =                                                              \
        std::regex_search(body.begin(), body.end(), matched,                   \
          StsResponseRegex::get().regex##X##);                                 \
    if (!result || !(matched.size() != 1)) {                                   \
      ENVOY_LOG(trace, "response body did not contain " #X);                   \
      context->callbacks()->onFailure(CredentialsFailureStatus::InvalidSts);   \
      return;                                                                  \
    }                                                                          \
    const auto &sub_match = matched[1];                                        \
    decltype(X) matched_sv(sub_match.first, sub_match.length());               \
    X = std::move(matched_sv);                                                 \
  }

  GET_PARAM(access_key);
  GET_PARAM(secret_key);
  GET_PARAM(session_token);
  GET_PARAM(expiration);

  SystemTime expiration_time;
  absl::Time absl_expiration_time;
  std::string error;
  if (absl::ParseTime(absl::RFC3339_sec, expiration,
                      &absl_expiration_time, &error)) {
    ENVOY_LOG(trace,
              "Determined expiration time from STS credentials result");
    expiration_time = absl::ToChronoTime(absl_expiration_time);
  } else {
    expiration_time = api_.timeSource().systemTime() + REFRESH_STS_CREDS;
    ENVOY_LOG(trace,
              "Unable to determine expiration time from STS credentials "
              "result (error: {}), using default",
              error);
  }

  StsCredentialsConstSharedPtr result =
      std::make_shared<const StsCredentials>(
          access_key, secret_key, session_token, expiration_time);

  for (auto&& ctx : connection_list_) {
    ctx.callbacks()->onSuccess(result);
  }
};

void StsConnectionPoolImpl::onFailure(CredentialsFailureStatus status) {
  for (auto&& ctx : connection_list_) {
    ctx.callbacks()->onFailure(status);
  }
};

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
