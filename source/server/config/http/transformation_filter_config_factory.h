#pragma once

#include <string>

#include "envoy/server/filter_config.h"

#include "transformation_filter.pb.h"

namespace Envoy {
namespace Server {
namespace Configuration {

class TransformationFilterConfigFactory : public NamedHttpFilterConfigFactory {
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

private:
  HttpFilterFactoryCb
  createFilter(const envoy::api::v2::filter::http::Transformations &proto_config,
               FactoryContext &context);

};

} // namespace Configuration
} // namespace Server
} // namespace Envoy
