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

  // ServerFactoryContext::clusterManager() is not available during server initialization
  // therefore we need to pass absl::nullopt in lieu of the server_context to prevent
  // the upstream code from attempting to access the method. https://github.com/envoyproxy/envoy/issues/26653
  auto chain = std::make_unique<Extensions::Common::Aws::DefaultCredentialsProviderChain>(
          server_context.api(), absl::nullopt /* ServerFactoryContextOptRef context */,
          server_context.singletonManager(),
          // We pass an empty string if we don't have a region
          proto_config.has_service_account_credentials() ? proto_config.service_account_credentials().region() : "",
          nullptr);
  auto sts_factory = StsCredentialsProviderFactory::create(server_context.api(),
                                            server_context.clusterManager());
  auto config = std::make_shared<AWSLambdaConfigImpl>(std::move(chain),
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
