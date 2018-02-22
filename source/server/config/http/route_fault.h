#pragma once

#include <string>

#include "envoy/server/filter_config.h"

#include "common/config/solo_well_known_names.h"

#include "route_fault.pb.h"

namespace Envoy {
namespace Server {
namespace Configuration {

/**
 * Config registration for the fault injection filter. @see
 * NamedHttpFilterConfigFactory.
 */
class RouteFaultFilterConfig : public NamedHttpFilterConfigFactory {
public:
  HttpFilterFactoryCb createFilterFactory(const Json::Object &json_config,
                                          const std::string &stats_prefix,
                                          FactoryContext &context) override;
  HttpFilterFactoryCb
  createFilterFactoryFromProto(const Protobuf::Message &proto_config,
                               const std::string &stats_prefix,
                               FactoryContext &context) override;

  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return ProtobufTypes::MessagePtr{
        new envoy::api::v2::filter::http::RouteFault()};
  }

  std::string name() override {
    return Config::SoloCommonFilterNames::get().ROUTE_FAULT;
  }

private:
  HttpFilterFactoryCb
  createFilter(const envoy::api::v2::filter::http::RouteFault &proto_config,
               const std::string &stats_prefix, FactoryContext &context);
};

} // namespace Configuration
} // namespace Server
} // namespace Envoy
