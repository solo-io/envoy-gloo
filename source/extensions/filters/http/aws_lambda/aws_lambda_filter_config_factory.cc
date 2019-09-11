#include "extensions/filters/http/aws_lambda/aws_lambda_filter_config_factory.h"

#include "envoy/registry/registry.h"

#include "extensions/filters/http/aws_lambda/aws_lambda_filter.h"

#include "api/envoy/config/filter/http/aws_lambda/v2/aws_lambda.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {

Http::FilterFactoryCb AWSLambdaFilterConfigFactory::createFilter(
    const std::string &, Server::Configuration::FactoryContext &context) {
  return [&context](Http::FilterChainFactoryCallbacks &callbacks) -> void {
    auto filter = new AWSLambdaFilter(context.clusterManager(),
                                      context.dispatcher().timeSource());
    callbacks.addStreamDecoderFilter(
        Http::StreamDecoderFilterSharedPtr{filter});
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

ProtobufTypes::MessagePtr
AWSLambdaFilterConfigFactory::createEmptyRouteConfigProto() {
  return std::make_unique<
      envoy::config::filter::http::aws_lambda::v2::AWSLambdaPerRoute>();
}

Router::RouteSpecificFilterConfigConstSharedPtr
AWSLambdaFilterConfigFactory::createRouteSpecificFilterConfig(
    const Protobuf::Message &config, Server::Configuration::FactoryContext &) {
  const auto &proto_config = dynamic_cast<
      const envoy::config::filter::http::aws_lambda::v2::AWSLambdaPerRoute &>(
      config);
  return std::make_shared<const AWSLambdaRouteConfig>(proto_config);
}

/**
 * Static registration for the AWS Lambda filter. @see RegisterFactory.
 */
REGISTER_FACTORY(AWSLambdaFilterConfigFactory, Server::Configuration::NamedHttpFilterConfigFactory);

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
