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

constexpr char EXPIRATION_FORMAT[] = "%E4Y%m%dT%H%M%S%z";


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
    // TODO: we probably need to catch and handle an exception here.
    const auto web_token = shared_this->api_.fileSystem().fileReadToEnd(shared_this->token_file_);
    // TODO: check if web_token is valid
    // TODO: stats here
    shared_this->tls_slot_->runOnAllThreads([shared_this, web_token](){
      auto tls_cache = shared_this->tls_slot_->getTyped<ThreadLocalStsCacheSharedPtr>();
      tls_cache->setWebToken(web_token);
    });
  });

}

void StsCredentialsProviderImpl::find(absl::optional<std::string> role_arn_arg, ContextSharedPtr context) {
  auto& ctximpl = static_cast<Context&>(*context);

  std::string role_arn = default_role_arn_;
  // If role_arn_arg is present, use that, otherwise use env
  if (role_arn_arg.has_value()) {
    role_arn = std::string(role_arn_arg.value());
  }

  ASSERT(!role_arn.empty());

  ENVOY_LOG(trace, "{}: Attempting to assume role ({})", __func__, role_arn);

  auto tls_cache = tls_slot_->getTyped<ThreadLocalStsCacheSharedPtr>();

  const auto it = tls_cache->credentialsCache().find(role_arn);
  if (it != tls_cache->credentialsCache().end()) {
    // thing  exists
    const auto now = api_.timeSource().systemTime();
    // If the expiration time is more than a minute away, return it immediately
    if (it->second->expirationTime() - now > REFRESH_GRACE_PERIOD) {
      ctximpl.callbacks()->onSuccess(it->second);
      return;        
    }
    // token is expired, fallthrough to create a new one
  }

  ctximpl.fetcher()->fetch(
    uri_, 
    role_arn, 
    tls_cache->webToken(), 
    [this, context, role_arn, &tls_cache](const std::string* body) {
      ASSERT(body != nullptr);

      //TODO: (yuval): create utility function for this regex search
      std::smatch matched_access_key;
      std::regex_search(*body, matched_access_key, access_key_regex_);
      if (!(matched_access_key.size() > 1)) {
        ENVOY_LOG(trace, "response body did not contain access_key");
        context->callbacks()->onFailure(CredentialsFailureStatus::InvalidSts);
      }

      std::smatch matched_secret_key;
      std::regex_search(*body, matched_secret_key, secret_key_regex_);
      if (!(matched_secret_key.size() > 1)) {
        ENVOY_LOG(trace, "response body did not contain secret_key");
        context->callbacks()->onFailure(CredentialsFailureStatus::InvalidSts);
      }
      
      std::smatch matched_session_token;
      std::regex_search(*body, matched_session_token, session_token_regex_);
      if (!(matched_session_token.size() > 1)) {
        ENVOY_LOG(trace, "response body did not contain session_token");
        context->callbacks()->onFailure(CredentialsFailureStatus::InvalidSts);
      }
      
      std::smatch matched_expiration;
      std::regex_search(*body, matched_expiration, expiration_regex_);
      if (!(matched_expiration.size() > 1)) {
        ENVOY_LOG(trace, "response body did not contain expiration");
        context->callbacks()->onFailure(CredentialsFailureStatus::InvalidSts);
      }

      SystemTime expiration_time;
      absl::Time absl_expiration_time;
      if (absl::ParseTime(EXPIRATION_FORMAT, matched_expiration[1].str(), &absl_expiration_time, nullptr)) {
        ENVOY_LOG(trace, "Determined expiration time from STS credentials result");
        expiration_time = absl::ToChronoTime(absl_expiration_time);
      } else {
        expiration_time = api_.timeSource().systemTime() + REFRESH_STS_CREDS;
        ENVOY_LOG(trace, "Unable to determine expiration time from STS credentials result, using default");
      }

      StsCredentialsConstSharedPtr result = std::make_shared<const StsCredentials>(matched_access_key[1].str(), matched_secret_key[1].str(), matched_session_token[1].str(), expiration_time);
      
      // Success callback, save back to cache
      tls_cache->credentialsCache().emplace(role_arn, result);
      context->callbacks()->onSuccess(result);
    },
    [this, context](CredentialsFailureStatus reason) {
      // unsuccessful, send back empty creds?
      context->callbacks()->onFailure(reason);
    }
  );
};

ContextSharedPtr ContextFactory::create(StsCredentialsProvider::Callbacks* callbacks) const {
  return std::make_shared<ContextImpl>(cm_, api_, callbacks);
};


} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
