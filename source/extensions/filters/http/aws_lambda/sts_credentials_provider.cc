#include "source/extensions/filters/http/aws_lambda/sts_credentials_provider.h"

#include "envoy/api/api.h"
#include "envoy/common/pure.h"
#include "envoy/common/time.h"

#include "source/common/common/linked_object.h"

#include "source/extensions/common/aws/credentials_provider.h"

#include "api/envoy/config/filter/http/aws_lambda/v2/aws_lambda.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {

class StsCredentialsProviderImpl : public StsCredentialsProvider,
                                   public StsConnectionPool::Callbacks,
                                   public Logger::Loggable<Logger::Id::aws> {

public:
  StsCredentialsProviderImpl(
      const envoy::config::filter::http::aws_lambda::v2::
          AWSLambdaConfig_ServiceAccountCredentials &config,
      Api::Api &api, Upstream::ClusterManager &cm,
      StsConnectionPoolFactoryPtr conn_pool_factory, std::string_view web_token,
      std::string_view role_arn);

  StsConnectionPool::Context *
  find(const absl::optional<std::string> &role_arn_arg,
        StsConnectionPool::Context::Callbacks *callbacks) override;

  void setWebToken(std::string_view web_token) override;

  void onResult(std::shared_ptr<const StsCredentials>,
              std::string role_arn, 
              std::list<std::string>  chained_requests) override;            

private:
  Api::Api &api_;
  Upstream::ClusterManager &cm_;
  const envoy::config::filter::http::aws_lambda::v2::
      AWSLambdaConfig_ServiceAccountCredentials config_;

  std::string default_role_arn_;
  envoy::config::core::v3::HttpUri uri_;
  StsConnectionPoolFactoryPtr conn_pool_factory_;

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
  Api::Api &api, Upstream::ClusterManager &cm,
  StsConnectionPoolFactoryPtr conn_pool_factory, std::string_view web_token,
  std::string_view role_arn)
  : api_(api), cm_(cm), config_(config), default_role_arn_(role_arn),
    conn_pool_factory_(std::move(conn_pool_factory)), web_token_(web_token) {
  

  uri_.set_cluster(config_.cluster());
  uri_.set_uri(config_.uri());
  // TODO: Figure out how to get this to compile, timeout is not all that
  // important right now uri_.set_allocated_timeout(config_.mutable_timeout())
  // Consider if this is still reasonable. We have a timeout configuration for
  // base webtoken handled in a filewatch in the config init.
  // We do want to decide about whether it matters to pass for chained.
  // Though chained can only have expiry of 15m to 60m and defaults to 60m.
}

void StsCredentialsProviderImpl::setWebToken(std::string_view web_token) {
  web_token_ = web_token;
}

void StsCredentialsProviderImpl::onResult(
    std::shared_ptr<const StsCredentials> result, std::string role_arn,
    std::list<std::string> chained_requests) {
  credentials_cache_.emplace(role_arn, result);
   while( !chained_requests.empty()){
    auto chained_role = chained_requests.back();
    ENVOY_LOG(trace, "calling sts chained for {}", chained_role);
    auto conn_pool = connection_pools_.find(chained_role);
    if (conn_pool != connection_pools_.end()) {
      conn_pool->second->init(uri_, "", result);
    }
    
    chained_requests.pop_back();
  }
}

StsConnectionPool::Context *StsCredentialsProviderImpl::find(
    const absl::optional<std::string> &role_arn_arg,
    StsConnectionPool::Context::Callbacks *callbacks) {

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
  auto conn_pool = connection_pools_.find(role_arn);
  if (conn_pool != connection_pools_.end()) {
    // We have an existing connection pool, 
    // check if there is already a request in flight
    if (conn_pool->second->requestInFlight()) {
     // add new context to connection pool and return it to the caller
     return conn_pool->second->add(callbacks);
    }
  }else{
     // since there is no existing connection pool then we need to create one
     conn_pool =  connection_pools_
          .emplace(role_arn, conn_pool_factory_->build(
              role_arn, this, StsFetcher::create(cm_, api_)))
          .first;
  }

  // Short circuit any additional checks if we are the base arn
  if (role_arn == default_role_arn_) {
    // initialize the connection and subscribe to the callbacks
    conn_pool->second->init(uri_, web_token_, NULL);
    return conn_pool->second->add(callbacks);
  }

  // For chaining check to see if we need to get the base token in addition.
  // Chained role expiration is not linked to base expiration so we just 
  // need to make sure the creds are good for the duration of our sts call.
  const auto existing_base_token = credentials_cache_.find(default_role_arn_); 
  if (existing_base_token != credentials_cache_.end()) {
    const auto now = api_.timeSource().systemTime();
    auto time_left = existing_base_token->second->expirationTime() - now;
    if (time_left > REFRESH_GRACE_PERIOD) {
      ENVOY_LOG(trace,"found base token with remaining time");
      conn_pool->second->init(uri_, web_token_, existing_base_token->second);
      return conn_pool->second->add(callbacks);
    }
  }
  
  // find/create default connection pool
  auto base_conn_pool = connection_pools_.find(default_role_arn_);
  if (base_conn_pool == connection_pools_.end()) {
     base_conn_pool =  connection_pools_
          .emplace(default_role_arn_, conn_pool_factory_->build(
            default_role_arn_, this, 
            StsFetcher::create(cm_, api_))).first;
  }
  // only recreate base request if its not in flight
  if (!base_conn_pool->second->requestInFlight()) {
   base_conn_pool->second->init(uri_, web_token_, NULL);
  }
  base_conn_pool->second->addChained(role_arn);


  // initialize the connection but dont fetch as we are waiting on base conn
  conn_pool->second->setInFlight();
  // generate and return a context with the current callbacks
  return conn_pool->second->add(callbacks);
};

class StsCredentialsProviderFactoryImpl : public StsCredentialsProviderFactory {
public:
  StsCredentialsProviderFactoryImpl(Api::Api &api, Upstream::ClusterManager &cm)
      : api_(api), cm_(cm){};

  StsCredentialsProviderPtr
  build(const envoy::config::filter::http::aws_lambda::v2::
            AWSLambdaConfig_ServiceAccountCredentials &config,
        Event::Dispatcher &dispatcher, std::string_view web_token,
        std::string_view role_arn) const override;

private:
  Api::Api &api_;
  Upstream::ClusterManager &cm_;
};

StsCredentialsProviderPtr StsCredentialsProviderFactoryImpl::build(
    const envoy::config::filter::http::aws_lambda::v2::
        AWSLambdaConfig_ServiceAccountCredentials &config,
    Event::Dispatcher &dispatcher, std::string_view web_token,
    std::string_view role_arn) const {

  return StsCredentialsProvider::create(
      config, api_, cm_, StsConnectionPoolFactory::create(api_, dispatcher),
      web_token, role_arn);
};

StsCredentialsProviderPtr StsCredentialsProvider::create(
    const envoy::config::filter::http::aws_lambda::v2::
        AWSLambdaConfig_ServiceAccountCredentials &config,
    Api::Api &api, Upstream::ClusterManager &cm,
    StsConnectionPoolFactoryPtr factory, std::string_view web_token,
    std::string_view role_arn) {

  return std::make_unique<StsCredentialsProviderImpl>(
      config, api, cm, std::move(factory), web_token, role_arn);
}

StsCredentialsProviderFactoryPtr
StsCredentialsProviderFactory::create(Api::Api &api, 
                                              Upstream::ClusterManager &cm) {
  return std::make_unique<StsCredentialsProviderFactoryImpl>(api, cm);
}

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
