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

class ContextImpl : public StsCredentialsProvider::Context {
public:
  ContextImpl(Upstream::ClusterManager& cm, Api::Api& api, StsCredentialsProvider::Callbacks* callback)
      : fetcher_(StsFetcher::create(cm, api)), callbacks_(callback) {}

  StsCredentialsProvider::Callbacks* callbacks() const override { return callbacks_; }
  StsFetcherPtr& fetcher() override { return fetcher_; }

  void cancel() override {
    fetcher_->cancel();
  }

private:
  StsFetcherPtr fetcher_;
  StsCredentialsProvider::Callbacks* callbacks_;
};

class StsCredentialsProviderImpl: public StsCredentialsProvider {
public:
  StsCredentialsProviderImpl(
    const envoy::config::filter::http::aws_lambda::v2::AWSLambdaConfig_ServiceAccountCredentials& config,
    Api::Api& api) : api_(api), config_(config), default_role_arn_(absl::NullSafeStringView(std::getenv(AWS_ROLE_ARN))),
    token_file_(absl::NullSafeStringView(std::getenv(AWS_WEB_IDENTITY_TOKEN_FILE))) {

    uri_.set_cluster(config_.cluster());
    uri_.set_uri(config_.uri());


    // File must exist on system
    if (!api_.fileSystem().fileExists(token_file_)) {
      throw EnvoyException("")
    }

    const auto web_token = api_.fileSystem().fileReadToEnd(token_file_);
    }

  void find(absl::optional<absl::string_view> role_arn_arg, ContextSharedPtr context){
    auto& ctximpl = static_cast<Context&>(*context);

    absl::string_view role_arn = default_role_arn_;
    // If role_arn_arg is present, use that, otherwise use env
    if (role_arn_arg.has_value()) {
      role_arn = role_arn_arg.value();
    }

    ASSERT(!role_arn.empty());

    const auto it = credentials_cache_.find(role_arn);
    if (it != credentials_cache_.end()) {
      // thing  exists
      // check for expired

      // TODO: if not expired, send back, otherwise don't return
      ctximpl.callbacks()->onSuccess(it->second);
      return;
      // return nullptr;
    }

    ASSERT(!token_file_.empty());
  
    uri.set_cluster(config_.cluster());
    uri.set_uri(config_.uri());
    // TODO: Figure out how to get this to compile, timeout is not all that important right now
    // uri.set_allocated_timeout(config_.mutable_timeout())
    ctximpl.fetcher()->fetch(
      uri_, 
      role_arn, 
      web_token, 
      [this, &ctximpl, role_arn](StsCredentialsConstSharedPtr& sts_credentials) {
        // Success callback, save back to cache
        credentials_cache_.emplace(role_arn, sts_credentials);
        ctximpl.callbacks()->onSuccess(sts_credentials);
      },
      [this, &ctximpl](CredentialsFailureStatus reason) {
        // unsuccessful, send back empty creds?
        ctximpl.callbacks()->onFailure(reason);
      }
    );
  };


private:

  Api::Api& api_;
  const envoy::config::filter::http::aws_lambda::v2::AWSLambdaConfig_ServiceAccountCredentials& config_;
  // Credentials storage map, keyed by arn
  std::unordered_map<std::string, StsCredentialsConstSharedPtr> credentials_cache_;

  std::string default_role_arn_;
  std::string token_file_;
  envoy::config::core::v3::HttpUri uri_;
};

ContextSharedPtr ContextFactory::create(StsCredentialsProvider::Callbacks* callbacks) const {
  return std::make_shared<ContextImpl>(cm_, api_, callbacks);
};

StsCredentialsProviderPtr StsCredentialsProvider::create(
  const envoy::config::filter::http::aws_lambda::v2::AWSLambdaConfig_ServiceAccountCredentials& config, Api::Api& api) {

  return std::make_unique<StsCredentialsProviderImpl>(config, api);
};

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
