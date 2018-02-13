#pragma once

#include <string>

#include "envoy/server/filter_config.h"

#include "lambda_filter.pb.h"

namespace Envoy {
namespace Server {
namespace Configuration {

class LambdaFilterConfigFactory : public NamedHttpFilterConfigFactory {
public:
  HttpFilterFactoryCb createFilterFactory(const Json::Object &config,
                                          const std::string &stat_prefix,
                                          FactoryContext &context) override;

  HttpFilterFactoryCb
  createFilterFactoryFromProto(const Protobuf::Message &config,
                               const std::string &stat_prefix,
                               FactoryContext &context) override;

  ProtobufTypes::MessagePtr createEmptyConfigProto() override;

  std::string name() override;
// no v1 support
//  static const envoy::api::v2::filter::http::Lambda
//  translateLambdaFilter(const Json::Object &json_config);

private:
  HttpFilterFactoryCb
  createFilter(const envoy::api::v2::filter::http::Lambda &proto_config,
               FactoryContext &context);

  static const std::string LAMBDA_HTTP_FILTER_SCHEMA;
};

} // namespace Configuration
} // namespace Server
} // namespace Envoy
