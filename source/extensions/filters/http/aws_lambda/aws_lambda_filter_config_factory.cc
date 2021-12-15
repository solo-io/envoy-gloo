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

  auto config = AWSLambdaConfigImpl::create(
      std::make_unique<
          Extensions::Common::Aws::DefaultCredentialsProviderChain>(
          context.api(), Extensions::Common::Aws::Utility::metadataFetcher),
      StsCredentialsProviderFactory::create(context.api(),
                                            context.clusterManager()),
      context.dispatcher(), context.api(), context.threadLocal(), stats_prefix,
      context.scope(), proto_config);
  auto should_propagate_origin = proto_config.propagate_original_routing().value();
  return
      [&context, should_propagate_origin, config](Http::FilterChainFactoryCallbacks &callbacks) -> void {
        callbacks.addStreamFilter(std::make_shared<AWSLambdaFilter>(
            context.clusterManager(), context.api(),
            should_propagate_origin, config));
      };
}

Upstream::ProtocolOptionsConfigConstSharedPtr
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

Router::RouteSpecificFilterConfigConstSharedPtr
AWSLambdaFilterConfigFactory::createRouteSpecificFilterConfigTyped(
    const envoy::config::filter::http::aws_lambda::v2::AWSLambdaPerRoute
        &proto_config,
    Server::Configuration::ServerFactoryContext &,
    ProtobufMessage::ValidationVisitor &) {
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
