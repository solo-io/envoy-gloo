#include <string>

#include "envoy/registry/registry.h"

#include "lambda_filter.h"

namespace Envoy {
namespace HTTP {
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

class LambdaFilterConfig
    : public Envoy::Server::Configuration::NamedHttpFilterConfigFactory {
public:
  Envoy::Server::Configuration::HttpFilterFactoryCb
  createFilterFactory(const Envoy::Json::Object &json_config,
                      const std::string &,
                      Envoy::Server::Configuration::FactoryContext &) override {
    json_config.validateSchema(LAMBDA_HTTP_FILTER_SCHEMA);

    std::string access_key = json_config.getString("access_key", "");
    std::string secret_key = json_config.getString("secret_key", "");

    ClusterFunctionMap functions = {
        {"lambda-func1",
         {"FunctionName", "lambda.us-east-1.amazonaws.com", "us-east-1"}}};

    return [access_key, secret_key, functions](
               Envoy::Http::FilterChainFactoryCallbacks &callbacks) -> void {
      auto filter = new LambdaFilter(
          std::move(access_key), std::move(secret_key), std::move(functions));
      callbacks.addStreamDecoderFilter(
          Envoy::Http::StreamDecoderFilterSharedPtr{filter});
    };
  }
  std::string name() override { return "lambda"; }
};

/**
 * Static registration for this sample filter. @see RegisterFactory.
 */
static Envoy::Registry::RegisterFactory<
    LambdaFilterConfig,
    Envoy::Server::Configuration::NamedHttpFilterConfigFactory>
    register_;

} // namespace Configuration
} // namespace HTTP
} // namespace Envoy
