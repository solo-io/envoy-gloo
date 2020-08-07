#include "extensions/filters/http/aws_lambda/sts_credentials_provider.h"

#include "envoy/api/api.h"
#include "envoy/common/pure.h"
#include "envoy/common/time.h"

#include "common/common/linked_object.h"

#include "extensions/common/aws/credentials_provider.h"

#include "api/envoy/config/filter/http/aws_lambda/v2/aws_lambda.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {

class StsCredentialsProviderImpl
    : public StsCredentialsProvider,
      public StsConnectionPool::Callbacks,
      public Logger::Loggable<Logger::Id::aws> {

public:
  StsCredentialsProviderImpl(
      const envoy::config::filter::http::aws_lambda::v2::
          AWSLambdaConfig_ServiceAccountCredentials &config,
      Api::Api &api, Event::Dispatcher &dispatcher, Upstream::ClusterManager &cm);

  StsConnectionPool::Context* find(const absl::optional<std::string> &role_arn_arg,
            StsConnectionPool::Context::Callbacks* callbacks) override;

  void setWebToken(std::string_view web_token) override;

  void onSuccess(std::shared_ptr<const StsCredentials>,
                 std::string_view role_arn) override;

private:

  Api::Api &api_;
  Event::Dispatcher &dispatcher_;
  Upstream::ClusterManager &cm_;
  const envoy::config::filter::http::aws_lambda::v2::
      AWSLambdaConfig_ServiceAccountCredentials config_;

  std::string default_role_arn_;
  envoy::config::core::v3::HttpUri uri_;

  std::regex regex_access_key_;
  std::regex regex_secret_key_;
  std::regex regex_session_token_;
  std::regex regex_expiration_;

  // web_token set by AWS, will be auto-updated by StsCredentialsProvider
  // TODO: udpate this file, inotify or timer
  std::string web_token_;
  // Credentials storage map, keyed by arn
  std::unordered_map<std::string, StsCredentialsConstSharedPtr>
      credentials_cache_;

  std::unordered_map<std::string, StsConnectionPoolPtr> connection_pools_;
};

StsCredentialsProviderImpl::StsCredentialsProviderImpl(
    const envoy::config::filter::http::aws_lambda::v2::
        AWSLambdaConfig_ServiceAccountCredentials &config,
    Api::Api &api, Event::Dispatcher &dispatcher, Upstream::ClusterManager &cm)
    : api_(api), dispatcher_(dispatcher), cm_(cm), config_(config),
      default_role_arn_(absl::NullSafeStringView(std::getenv(AWS_ROLE_ARN))) {
      // file_watcher_(dispatcher.createFilesystemWatcher()) {

  uri_.set_cluster(config_.cluster());
  uri_.set_uri(config_.uri());
  // TODO: Figure out how to get this to compile, timeout is not all that
  // important right now uri_.set_allocated_timeout(config_.mutable_timeout())

  // AWS_WEB_IDENTITY_TOKEN_FILE and AWS_ROLE_ARN must be set for STS
  // credentials to be enabled
  std::string token_file = std::string(absl::NullSafeStringView(std::getenv(AWS_WEB_IDENTITY_TOKEN_FILE)));
  if (token_file == "") {
    throw EnvoyException(fmt::format("Env var {} must be present, and set",
                                     AWS_WEB_IDENTITY_TOKEN_FILE));
  }
  if (default_role_arn_ == "") {
    throw EnvoyException(
        fmt::format("Env var {} must be present, and set", AWS_ROLE_ARN));
  }
  // File must exist on system
  if (!api_.fileSystem().fileExists(token_file)) {
    throw EnvoyException(
        fmt::format("Web token file {} does not exist", token_file));
  }

  web_token_ = api_.fileSystem().fileReadToEnd(token_file);
  // File should not be empty
  if (web_token_ == "") {
    throw EnvoyException(
        fmt::format("Web token file {} exists but is empty", token_file));
  }
}

void StsCredentialsProviderImpl::setWebToken(std::string_view web_token) {
  web_token_ = web_token;
}

void StsCredentialsProviderImpl::onSuccess(
    std::shared_ptr<const StsCredentials> result, std::string_view role_arn) {
  credentials_cache_.emplace(role_arn, result);
}

StsConnectionPool::Context* StsCredentialsProviderImpl::find(const absl::optional<std::string> &role_arn_arg, 
            StsConnectionPool::Context::Callbacks* callbacks) {

  std::string role_arn = default_role_arn_;
  // If role_arn_arg is present, use that, otherwise use env
  if (role_arn_arg.has_value()) {
    role_arn = role_arn_arg.value();
  }

  ASSERT(!role_arn.empty());

  ENVOY_LOG(trace, "{}: Attempting to assume role ({})", __func__, role_arn);

  const auto existing_token = credentials_cache_.find(role_arn);
  if (existing_token != credentials_cache_.end()) {
    // thing  exists
    const auto now = api_.timeSource().systemTime();
    // If the expiration time is more than a minute away, return it immediately
    auto time_left = existing_token->second->expirationTime() - now;
    if (time_left > REFRESH_GRACE_PERIOD) {
      callbacks->onSuccess(existing_token->second);
      return nullptr;
    }
    // token is expired, fallthrough to create a new one
  }

  // Look for active connection pool for given role_arn
  const auto existing_pool = connection_pools_.find(role_arn);
  if (existing_pool != connection_pools_.end()) {
    // We have an existing connection pool, add new context to connection pool and return it to the caller
    return existing_pool->second->add(callbacks);
  }

  // No pool exists, create a new one
  auto conn_pool = StsConnectionPool::create(cm_, api_, dispatcher_, role_arn, this);
  // Add the new pool to our list of active pools
  connection_pools_.emplace(role_arn, conn_pool);
  // initialize the connection
  conn_pool->init(uri_, web_token_);
  // generate and return a context with the current callbacks
  return conn_pool->add(callbacks);
};


class StsCredentialsProviderFactoryImpl : public StsCredentialsProviderFactory {
public:
  StsCredentialsProviderFactoryImpl(Api::Api &api,
                                    Upstream::ClusterManager &cm)
      : api_(api), cm_(cm) {};

  StsCredentialsProviderPtr
  build(const envoy::config::filter::http::aws_lambda::v2::
             AWSLambdaConfig_ServiceAccountCredentials &config, 
                                    Event::Dispatcher &dispatcher) const override;

private:
  Api::Api &api_;
  Upstream::ClusterManager &cm_;
};

StsCredentialsProviderPtr StsCredentialsProviderFactoryImpl::build(
    const envoy::config::filter::http::aws_lambda::v2::
        AWSLambdaConfig_ServiceAccountCredentials &config,
                                    Event::Dispatcher &dispatcher) const {

  return StsCredentialsProvider::create(config, api_, dispatcher, cm_);
};


StsCredentialsProviderPtr StsCredentialsProvider::create(const envoy::config::filter::http::aws_lambda::v2::
             AWSLambdaConfig_ServiceAccountCredentials &config,
         Api::Api &api, Event::Dispatcher &dispatcher, Upstream::ClusterManager &cm) {
  return std::make_shared<StsCredentialsProviderImpl>(config, api, dispatcher, cm);
}

StsCredentialsProviderFactoryPtr StsCredentialsProviderFactory::create(Api::Api &api, Upstream::ClusterManager &cm) {
  return std::make_unique<StsCredentialsProviderFactoryImpl>(api, cm);
}

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
