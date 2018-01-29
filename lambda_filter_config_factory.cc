#include "lambda_filter_config_factory.h"

#include <string>

#include "envoy/registry/registry.h"

#include "common/common/macros.h"
#include "common/config/json_utility.h"
#include "common/protobuf/utility.h"

#include "function.h"
#include "lambda_filter.h"
#include "lambda_filter.pb.h"
#include "lambda_filter_config.h"
#include "metadata_function_retriever.h"

namespace Envoy {
namespace Server {
namespace Configuration {

HttpFilterFactoryCb
LambdaFilterConfigFactory::createFilterFactory(const Json::Object &config,
                                               const std::string &stat_prefix,
                                               FactoryContext &context) {
  UNREFERENCED_PARAMETER(stat_prefix);

  return createFilter(translateLambdaFilter(config), context);
}

HttpFilterFactoryCb LambdaFilterConfigFactory::createFilterFactoryFromProto(
    const Protobuf::Message &config, const std::string &stat_prefix,
    FactoryContext &context) {
  UNREFERENCED_PARAMETER(stat_prefix);

  /**
   * TODO:
   * The corresponding `.pb.validate.h` for the message is required by
   * Envoy::MessageUtil.
   * @see https://github.com/envoyproxy/envoy/pull/2194
   *
   * #include "lambda_filter.pb.validate.h"
   *
   * return createFilter(
   *    Envoy::MessageUtil::downcastAndValidate<const
   * envoy::api::v2::filter::http::Lambda&>(proto_config), context);
   * */

  return createFilter(
      dynamic_cast<const envoy::api::v2::filter::http::Lambda &>(config),
      context);
}

ProtobufTypes::MessagePtr LambdaFilterConfigFactory::createEmptyConfigProto() {
  return ProtobufTypes::MessagePtr{new envoy::api::v2::filter::http::Lambda()};
}

std::string LambdaFilterConfigFactory::name() { return "io.solo.lambda"; }

const envoy::api::v2::filter::http::Lambda
LambdaFilterConfigFactory::translateLambdaFilter(
    const Json::Object &json_config) {
  json_config.validateSchema(LAMBDA_HTTP_FILTER_SCHEMA);

  envoy::api::v2::filter::http::Lambda proto_config;
  JSON_UTIL_SET_STRING(json_config, proto_config, access_key);
  JSON_UTIL_SET_STRING(json_config, proto_config, secret_key);

  return proto_config;
}

HttpFilterFactoryCb LambdaFilterConfigFactory::createFilter(
    const envoy::api::v2::filter::http::Lambda &proto_config,
    FactoryContext &context) {

  Http::LambdaFilterConfigSharedPtr config =
      std::make_shared<Http::LambdaFilterConfig>(proto_config);

  Http::FunctionRetrieverSharedPtr functionRetriever =
      std::make_shared<Http::MetadataFunctionRetriever>();

  return [&context, config, functionRetriever](
             Envoy::Http::FilterChainFactoryCallbacks &callbacks) -> void {
    auto filter = new Http::LambdaFilter(config, functionRetriever,
                                         context.clusterManager());
    callbacks.addStreamDecoderFilter(
        Http::StreamDecoderFilterSharedPtr{filter});
  };
}

const std::string LambdaFilterConfigFactory::LAMBDA_HTTP_FILTER_SCHEMA(R"EOF(
  {
    "$schema": "http://json-schema.org/schema#",
    "type" : "object",
    "properties" : {
      "access_key": {
        "type" : "string"
      },
      "secret_key": {
        "type" : "string"
      }
    },
    "required": ["access_key", "secret_key"],
    "additionalProperties" : false
  }
  )EOF");

/**
 * Static registration for this sample filter. @see RegisterFactory.
 */
static Envoy::Registry::RegisterFactory<
    LambdaFilterConfigFactory,
    Envoy::Server::Configuration::NamedHttpFilterConfigFactory>
    register_;

} // namespace Configuration
} // namespace Server
} // namespace Envoy
