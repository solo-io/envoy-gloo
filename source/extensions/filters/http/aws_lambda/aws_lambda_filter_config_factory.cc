#include "source/extensions/filters/http/aws_lambda/aws_lambda_filter_config_factory.h"

#include "envoy/registry/registry.h"

#include "source/extensions/common/aws/credentials_provider_impl.h"
#include "source/extensions/common/aws/utility.h"
#include "source/extensions/filters/http/aws_lambda/aws_lambda_filter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {

Http::FilterFactoryCb
AWSLambdaFilterConfigFactory::createFilterFactoryFromProtoTyped(
    const envoy::config::filter::http::aws_lambda::v2::AWSLambdaConfig
        &proto_config,
    const std::string &stats_prefix,
    Server::Configuration::FactoryContext &context) {
  auto& server_context = context.serverFactoryContext();

  // Get the region from the service account credentials if available
  std::string region = "";
  if (proto_config.has_service_account_credentials()) {
    region = proto_config.service_account_credentials().region();
  }

  // Create the appropriate credentials provider based on configuration
  Extensions::Common::Aws::CredentialsProviderSharedPtr credentials_provider;
  
  // If credentials are set in the config, use them directly
  if (proto_config.has_credentials()) {
    ENVOY_LOG(debug, "Using credentials from filter configuration");
    const auto& config_credentials = proto_config.credentials();
    credentials_provider = std::make_shared<Extensions::Common::Aws::ConfigCredentialsProvider>(
        config_credentials.access_key_id(), config_credentials.secret_access_key(),
        config_credentials.session_token());
  }
  // If credentials profile is specified, use the credentials file provider
  else if (!proto_config.credentials_profile().empty()) {
    ENVOY_LOG(debug, "Using credentials profile: {}", proto_config.credentials_profile());
    envoy::extensions::common::aws::v3::CredentialsFileCredentialProvider credential_file_config;
    credential_file_config.set_profile(proto_config.credentials_profile());
    credentials_provider = std::make_shared<Extensions::Common::Aws::CredentialsFileCredentialsProvider>(
        server_context, credential_file_config);
  }
  // Otherwise use the default credentials provider chain
  else {
    ENVOY_LOG(debug, "Using default credentials provider chain");
    credentials_provider = std::make_shared<Extensions::Common::Aws::DefaultCredentialsProviderChain>(
        server_context.api(), server_context, region, nullptr);
  }

  auto sts_factory = StsCredentialsProviderFactory::create(server_context.api(),
                                            server_context.clusterManager());
  auto config = std::make_shared<AWSLambdaConfigImpl>(std::move(credentials_provider),
      std::move(sts_factory),
      server_context.mainThreadDispatcher(), server_context.api(), server_context.threadLocal(), stats_prefix,
      server_context.scope(), proto_config);
  return
      [&server_context, config]
      (Http::FilterChainFactoryCallbacks &callbacks) -> void {
        callbacks.addStreamFilter(std::make_shared<AWSLambdaFilter>(
            server_context.clusterManager(), server_context.api(), config));
      };
}

absl::StatusOr<Upstream::ProtocolOptionsConfigConstSharedPtr>
AWSLambdaFilterConfigFactory::createProtocolOptionsConfig(
    const Protobuf::Message &config,
    Server::Configuration::ProtocolOptionsFactoryContext &) {
  const auto &proto_config =
      dynamic_cast<const envoy::config::filter::http::aws_lambda::v2::
                       AWSLambdaProtocolExtension &>(config);
  return std::make_shared<const AWSLambdaProtocolExtensionConfig>(proto_config);
}

ProtobufTypes::MessagePtr
AWSLambdaFilterConfigFactory::createEmptyProtocolOptionsProto() {
  return std::make_unique<envoy::config::filter::http::aws_lambda::v2::
                              AWSLambdaProtocolExtension>();
}

absl::StatusOr<Router::RouteSpecificFilterConfigConstSharedPtr>
AWSLambdaFilterConfigFactory::createRouteSpecificFilterConfigTyped(
    const envoy::config::filter::http::aws_lambda::v2::AWSLambdaPerRoute
        &proto_config,
    Server::Configuration::ServerFactoryContext &context,
    ProtobufMessage::ValidationVisitor &) {
  return std::make_shared<const AWSLambdaRouteConfig>(proto_config, context);
}

/**
 * Static registration for the AWS Lambda filter. @see RegisterFactory.
 */
REGISTER_FACTORY(AWSLambdaFilterConfigFactory,
                 Server::Configuration::NamedHttpFilterConfigFactory);

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
