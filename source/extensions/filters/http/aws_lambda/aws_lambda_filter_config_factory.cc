#include "extensions/filters/http/aws_lambda/aws_lambda_filter_config_factory.h"

#include "envoy/registry/registry.h"

#include "extensions/filters/http/aws_lambda/aws_lambda_filter.h"

#include "api/envoy/config/filter/http/aws_lambda/v2/aws_lambda.pb.validate.h"

namespace Envoy {
namespace Server {
namespace Configuration {

Http::FilterFactoryCb
AWSLambdaFilterConfigFactory::createFilter(const std::string &,
                                           FactoryContext &context) {
  return [&context](Http::FilterChainFactoryCallbacks &callbacks) -> void {
    auto filter = new Http::AWSLambdaFilter(context.clusterManager());
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
  return std::make_shared<const Http::AWSLambdaProtocolExtensionConfig>(
      proto_config);
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
    const Protobuf::Message &config, FactoryContext &) {
  const auto &proto_config = dynamic_cast<
      const envoy::config::filter::http::aws_lambda::v2::AWSLambdaPerRoute &>(
      config);
  return std::make_shared<const Http::AWSLambdaRouteConfig>(proto_config);
}

/**
 * Static registration for the AWS Lambda filter. @see RegisterFactory.
 */
static Registry::RegisterFactory<AWSLambdaFilterConfigFactory,
                                 NamedHttpFilterConfigFactory>
    register_;

} // namespace Configuration
} // namespace Server
} // namespace Envoy
