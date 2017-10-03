#include <string>

#include "http_filter.h"

#include "envoy/registry/registry.h"

namespace Solo {
namespace Lambda {
namespace Configuration {
  
const std::string lAMBDA_HTTP_FILTER_SCHEMA(R"EOF(
  {
    "$schema": "http://json-schema.org/schema#",
    "type" : "object",
    "properties" : {
      "access_key": {
        "type" : "string"
      },
      "secret_key": {
        "type" : "string"
      },
      "functions": {
        "type" : "object",
        "additionalProperties" : {
          "type" : "object",
          "properties": {
            "func_name" : {"type":"string"},
            "hostname" : {"type":"string"},
            "region" : {"type":"string"}
          }
        }
      }
    },
    "required": ["access_key", "secret_key", "functions"],
    "additionalProperties" : false
  }
  )EOF");

class LambdaFilterConfig : public NamedHttpFilterConfigFactory {
public:
  HttpFilterFactoryCb createFilterFactory(const Json::Object& json_config, const std::string&,
                                          FactoryContext& factoryContext) override {
    json_config.validateSchema(lAMBDA_HTTP_FILTER_SCHEMA);
                     
  std::string access_key = json_config.getString("access_key", "");
  std::string secret_key = json_config.getString("secret_key", "");
  const Json::ObjectSharedPtr functions_obj = json_config.getObject("functions", false);

  Http::ClusterFunctionMap functions;

  functions_obj->iterate([&functions](const std::string& key, const Json::Object& value){
    const std::string cluster_name = key;
    const std::string func_name = value.getString("func_name", "");
    const std::string hostname = value.getString("hostname", "");
    const std::string region = value.getString("region", "");
    functions[cluster_name] = Http::Function {
      func_name_ : func_name,
      hostname_: hostname,
      region_ : region,
    };
    return true;
  });

    return [&factoryContext, access_key, secret_key, functions](Http::FilterChainFactoryCallbacks& callbacks) -> void {
      auto filter = new Lambda::LambdaFilter(std::move(access_key), std::move(secret_key), std::move(functions));
      callbacks.addStreamDecoderFilter(
          Http::StreamDecoderFilterSharedPtr{filter});
    };
  }
  std::string name() override { return "lambda"; }
};

/**
 * Static registration for this sample filter. @see RegisterFactory.
 */
static Registry::RegisterFactory<LambdaFilterConfig, NamedHttpFilterConfigFactory>
    register_;

} // Configuration
} // Lambda
} // Solo
