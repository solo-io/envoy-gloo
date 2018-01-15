#include <string>

#include "envoy/registry/registry.h"

#include "common/config/json_utility.h"
#include "common/protobuf/utility.h"

#include "lambda_filter.h"
#include "lambda_filter.pb.h"

namespace Envoy {
namespace Server {
namespace Configuration {

const std::string LAMBDA_HTTP_FILTER_SCHEMA(R"EOF(
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

class LambdaFilterConfigFactory : public NamedHttpFilterConfigFactory {
public:
  HttpFilterFactoryCb createFilterFactory(const Json::Object &json_config,
                                          const std::string &,
                                          FactoryContext &context) override {

    envoy::api::v2::filter::http::Lambda proto_config;
    translateLambdaFilter(json_config, proto_config);

    return createFilter(proto_config, context);
  }

  HttpFilterFactoryCb
  createFilterFactoryFromProto(const Protobuf::Message &proto_config,
                               const std::string &,
                               FactoryContext &context) override {
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
        dynamic_cast<const envoy::api::v2::filter::http::Lambda &>(
            proto_config),
        context);
  }

  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return ProtobufTypes::MessagePtr{
        new envoy::api::v2::filter::http::Lambda()};
  }

  std::string name() override { return "lambda"; }

private:
  HttpFilterFactoryCb
  createFilter(const envoy::api::v2::filter::http::Lambda &proto_config,
               FactoryContext &context) {

    Http::LambdaFilterConfigSharedPtr config =
        std::make_shared<Http::LambdaFilterConfig>(
            Http::LambdaFilterConfig(proto_config));

    Http::ClusterFunctionMap functions = {
        {"lambda-func1",
         {"FunctionName", "lambda.us-east-1.amazonaws.com", "us-east-1"}}};

    return [&context, config, functions](
               Envoy::Http::FilterChainFactoryCallbacks &callbacks) -> void {
      auto filter = new Http::LambdaFilter(config, std::move(functions));
      callbacks.addStreamDecoderFilter(
          Http::StreamDecoderFilterSharedPtr{filter});
    };
  }

  void
  translateLambdaFilter(const Json::Object &json_config,
                        envoy::api::v2::filter::http::Lambda &proto_config) {

    json_config.validateSchema(LAMBDA_HTTP_FILTER_SCHEMA);

    JSON_UTIL_SET_STRING(json_config, proto_config, access_key);
    JSON_UTIL_SET_STRING(json_config, proto_config, secret_key);
  }
};

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
