#include "extensions/filters/http/aws_lambda/sts_credentials_provider.h"

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
  
/*
  * AssumeRoleWithIdentity returns a set of temporary credentials with a minimum lifespan of 15 minutes.
  * https://docs.aws.amazon.com/STS/latest/APIReference/API_AssumeRoleWithWebIdentity.html
  * 
  * In order to ensure that credentials never expire, we default to 2/3.
  * 
  * This in combination with the very generous grace period which makes sure the tokens are 
  * refreshed if they have < 5 minutes left on their lifetime. Whether that lifetime is 
  * our prescribed, or from the response itself.
*/
constexpr std::chrono::milliseconds REFRESH_STS_CREDS =
    std::chrono::minutes(10);

constexpr std::chrono::minutes REFRESH_GRACE_PERIOD{5};

class ContextImpl : public StsCredentialsProvider::Context {
public:
  ContextImpl(Upstream::ClusterManager& cm, Api::Api& api, StsCredentialsProvider::Callbacks* callback)
      : fetcher_(StsFetcher::create(cm, api)), callbacks_(callback) {}

  StsCredentialsProvider::Callbacks* callbacks() const override { return callbacks_; }
  StsFetcher& fetcher() override { return *fetcher_; }

  void cancel() override {
    fetcher_->cancel();
  }

private:
  StsFetcherPtr fetcher_;
  StsCredentialsProvider::Callbacks* callbacks_;
};


void StsCredentialsProviderImpl::init() {
  // Add file watcher for token file
  auto shared_this = shared_from_this();
  file_watcher_->addWatch(token_file_, Filesystem::Watcher::Events::Modified, [shared_this](uint32_t) {
    try {
      const auto web_token = shared_this->api_.fileSystem().fileReadToEnd(shared_this->token_file_);
      // TODO: check if web_token is valid
      // TODO: stats here
      shared_this->tls_slot_->runOnAllThreads([shared_this, web_token](){
        auto& tls_cache = shared_this->tls_slot_->getTyped<ThreadLocalStsCache>();
        tls_cache.setWebToken(web_token);
      });
  } catch (const EnvoyException& e) {
      ENVOY_LOG_TO_LOGGER(Envoy::Logger::Registry::getLog(Logger::Id::aws), warn, "{}: Exception while reading file during watch ({}): {}", __func__, shared_this->token_file_, e.what());
  }
  });
}

void StsCredentialsProviderImpl::find(const absl::optional<std::string> & role_arn_arg, ContextSharedPtr context) {
  auto& ctximpl = static_cast<Context&>(*context);

  std::string role_arn = default_role_arn_;
  // If role_arn_arg is present, use that, otherwise use env
  if (role_arn_arg.has_value()) {
    role_arn = role_arn_arg.value();
  }

  ASSERT(!role_arn.empty());

  ENVOY_LOG(trace, "{}: Attempting to assume role ({})", __func__, role_arn);

  auto& tls_cache = tls_slot_->getTyped<ThreadLocalStsCache>();
  auto& credential_cache = tls_cache.credentialsCache();
  const auto it = credential_cache.find(role_arn);
  if (it != credential_cache.end()) {
    // thing  exists
    const auto now = api_.timeSource().systemTime();
    // If the expiration time is more than a minute away, return it immediately
    auto time_left = it->second->expirationTime() - now;
    if (time_left > REFRESH_GRACE_PERIOD) {
      ctximpl.callbacks()->onSuccess(it->second);
      return;
    }
    // token is expired, fallthrough to create a new one
  }

  ctximpl.fetcher().fetch(
    uri_, 
    role_arn, 
    tls_cache.webToken(), 
    [this, context, role_arn](const absl::string_view body) {
      ASSERT(body != nullptr);

      // using a macro as we need to return on error
      // TODO(yuval-k): we can use string_view instead of string when we upgrade to newer absl.
      #define GET_PARAM(X) std::string X; \
      { \
      std::match_results<absl::string_view::const_iterator> matched; \
      bool result = std::regex_search(body.begin(), body.end(), matched, regex_##X##_); \
      if (!result || !(matched.size() != 1)) { \
        ENVOY_LOG(trace, "response body did not contain " #X); \
        context->callbacks()->onFailure(CredentialsFailureStatus::InvalidSts); \
        return; \
      } \
      const auto &sub_match = matched[1]; \
      decltype(X) matched_sv(sub_match.first, sub_match.length()); \
      X = std::move(matched_sv); \
      }

      GET_PARAM(access_key);
      GET_PARAM(secret_key);
      GET_PARAM(session_token);
      GET_PARAM(expiration);

      SystemTime expiration_time;
      absl::Time absl_expiration_time;
      std::string error;
      if (absl::ParseTime(absl::RFC3339_sec, expiration, &absl_expiration_time, &error)) {
        ENVOY_LOG(trace, "Determined expiration time from STS credentials result");
        expiration_time = absl::ToChronoTime(absl_expiration_time);
      } else {
        expiration_time = api_.timeSource().systemTime() + REFRESH_STS_CREDS;
        ENVOY_LOG(trace, "Unable to determine expiration time from STS credentials result (error: {}), using default", error);
      }

      StsCredentialsConstSharedPtr result = std::make_shared<const StsCredentials>(access_key, secret_key, session_token, expiration_time);
      
      // Success callback, save back to cache
      auto& tls_cache = tls_slot_->getTyped<ThreadLocalStsCache>();
      auto& credential_cache = tls_cache.credentialsCache();
      credential_cache.emplace(role_arn, result);
      context->callbacks()->onSuccess(result);
    },
    [context](CredentialsFailureStatus reason) {
      // unsuccessful, send back empty creds?
      context->callbacks()->onFailure(reason);
    }
  );
};

ContextSharedPtr ContextFactory::create(StsCredentialsProvider::Callbacks* callbacks) const {
  return std::make_shared<ContextImpl>(cm_, api_, callbacks);
};

StsCredentialsProviderPtr StsCredentialsProviderFactoryImpl::create(
    const envoy::config::filter::http::aws_lambda::v2::AWSLambdaConfig_ServiceAccountCredentials& config) const {

  return StsCredentialsProviderImpl::create(config, api_, tls_, dispatcher_);
};

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
