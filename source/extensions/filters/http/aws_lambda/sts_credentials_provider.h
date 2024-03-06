#pragma once

#include "envoy/api/api.h"
#include "envoy/common/pure.h"
#include "envoy/common/time.h"

#include "source/common/common/regex.h"

#include "source/extensions/common/aws/credentials_provider.h"
#include "source/extensions/filters/http/aws_lambda/sts_connection_pool.h"

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
      bool disable_role_chaining,
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

  Envoy::Extensions::Common::Aws::CredentialsProviderSharedPtr createWebIdentityCredentialsProvider(
    Api::Api& api, Server::Configuration::ServerFactoryContext context,
    const Envoy::Extensions::Common::Aws::MetadataCredentialsProviderBase::CurlMetadataFetcher& fetch_metadata_using_curl,
    Envoy::Extensions::Common::Aws::CreateMetadataFetcherCb create_metadata_fetcher_cb, absl::string_view cluster_name,
    absl::string_view token_file_path, absl::string_view sts_endpoint, absl::string_view role_arn,
    absl::string_view role_session_name) const override {
    std::cout << "we have hit createWebIdentityCredentialsProvider" <<std::endl;
  };

};

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
