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


  auto chain = std::make_unique<Extensions::Common::Aws::DefaultCredentialsProviderChain>(
          context.serverFactoryContext().api(), Extensions::Common::Aws::Utility::fetchMetadata);
  auto sts_factory = StsCredentialsProviderFactory::create(context.serverFactoryContext().api(),
                                            context.serverFactoryContext().clusterManager());
  auto config = std::make_shared<AWSLambdaConfigImpl>(std::move(chain),
      std::move(sts_factory),
      context.serverFactoryContext().mainThreadDispatcher(), context.serverFactoryContext().api(), context.serverFactoryContext().threadLocal(), stats_prefix,
      context.serverFactoryContext().scope(), proto_config);
  return
      [&context, config]
      (Http::FilterChainFactoryCallbacks &callbacks) -> void {
        callbacks.addStreamFilter(std::make_shared<AWSLambdaFilter>(
            context.serverFactoryContext().clusterManager(), context.serverFactoryContext().api(), config));
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
