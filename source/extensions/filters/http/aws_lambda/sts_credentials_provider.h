#pragma once

#include "envoy/api/api.h"
#include "envoy/common/pure.h"
#include "envoy/common/time.h"

#include "common/common/regex.h"

#include "extensions/common/aws/credentials_provider.h"
#include "extensions/filters/http/aws_lambda/sts_connection_pool.h"

#include "api/envoy/config/filter/http/aws_lambda/v2/aws_lambda.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {

namespace {
constexpr char AWS_ROLE_ARN[] = "AWS_ROLE_ARN";
constexpr char AWS_WEB_IDENTITY_TOKEN_FILE[] = "AWS_WEB_IDENTITY_TOKEN_FILE";

constexpr std::chrono::minutes REFRESH_GRACE_PERIOD{5};
} // namespace

class StsCredentialsProvider;
using StsCredentialsProviderPtr = std::unique_ptr<StsCredentialsProvider>;

class StsCredentialsProvider {
public:
  virtual ~StsCredentialsProvider() = default;

  // Lookup credentials cache map.
  virtual StsConnectionPool::Context *
  find(const absl::optional<std::string> &role_arn,
       StsConnectionPool::Context::Callbacks *callbacks) PURE;

  virtual void setWebToken(std::string_view web_token) PURE;

  static StsCredentialsProviderPtr
  create(const envoy::config::filter::http::aws_lambda::v2::
             AWSLambdaConfig_ServiceAccountCredentials &config,
         Api::Api &api, Upstream::ClusterManager &cm,
         StsConnectionPoolFactoryPtr factory, std::string_view web_token,
         std::string_view role_arn);
};

class StsCredentialsProviderFactory;
using StsCredentialsProviderFactoryPtr =
    std::unique_ptr<StsCredentialsProviderFactory>;

class StsCredentialsProviderFactory {
public:
  virtual ~StsCredentialsProviderFactory() = default;

  virtual StsCredentialsProviderPtr
  build(const envoy::config::filter::http::aws_lambda::v2::
            AWSLambdaConfig_ServiceAccountCredentials &config,
        Event::Dispatcher &dispatcher, std::string_view web_token,
        std::string_view role_arn) const PURE;

  static StsCredentialsProviderFactoryPtr create(Api::Api &api,
                                                 Upstream::ClusterManager &cm);
};

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
