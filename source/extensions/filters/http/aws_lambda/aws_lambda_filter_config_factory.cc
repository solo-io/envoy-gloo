#include "extensions/filters/http/aws_lambda/aws_lambda_filter_config_factory.h"

#include "envoy/registry/registry.h"

#include "extensions/filters/http/aws_lambda/aws_lambda_filter.h"
#include "extensions/filters/http/common/aws/credentials_provider_impl.h"
#include "extensions/filters/http/common/aws/utility.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {

Http::FilterFactoryCb
AWSLambdaFilterConfigFactory::createFilterFactoryFromProtoTyped(
    const envoy::config::filter::http::aws_lambda::v2::AWSLambdaConfig
        &proto_config,
    const std::string &, Server::Configuration::FactoryContext &context) {

  auto config = std::make_shared<AWSLambdaConfigImpl>(
      std::make_unique<Common::Aws::DefaultCredentialsProviderChain>(
          context.api(), HttpFilters::Common::Aws::Utility::metadataFetcher),
      context.dispatcher(), context.threadLocal(), proto_config);

  return [&context,
          config](Http::FilterChainFactoryCallbacks &callbacks) -> void {
    callbacks.addStreamDecoderFilter(std::make_shared<AWSLambdaFilter>(
        context.clusterManager(), context.dispatcher().timeSource(), config));
  };
}

Upstream::ProtocolOptionsConfigConstSharedPtr
AWSLambdaFilterConfigFactory::createProtocolOptionsConfig(
    const Protobuf::Message &config) {
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

Router::RouteSpecificFilterConfigConstSharedPtr
AWSLambdaFilterConfigFactory::createRouteSpecificFilterConfigTyped(
    const envoy::config::filter::http::aws_lambda::v2::AWSLambdaPerRoute
        &proto_config,
    Server::Configuration::FactoryContext &) {
  return std::make_shared<const AWSLambdaRouteConfig>(proto_config);
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
