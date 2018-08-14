#pragma once

#include "common/config/solo_well_known_names.h"
#include "common/http/filter/fault_filter.h"
#include "common/http/route_enabled_filter_wrapper.h"

#include "route_fault.pb.h"

namespace Envoy {
namespace Http {

class RouteFaultFilterConfig {

  using ProtoConfig = envoy::api::v2::filter::http::RouteFault;

public:
  RouteFaultFilterConfig(ProtoConfig proto_config);

  const std::string &name() { return proto_config_.fault_name(); }

private:
private:
  ProtoConfig proto_config_;
};

typedef std::shared_ptr<RouteFaultFilterConfig> RouteFaultFilterConfigSharedPtr;

class RouteFaultFilter : public RouteEnabledFilterWrapper<FaultFilter> {
public:
  RouteFaultFilter(RouteFaultFilterConfigSharedPtr route_filter_config,
                   FaultFilterConfigSharedPtr filter_config)
      : RouteEnabledFilterWrapper<FaultFilter>(
            Config::SoloCommonFilterNames::get().ROUTE_FAULT, filter_config),
        route_filter_config_(route_filter_config) {}

protected:
  virtual bool
  shouldActivate(const ProtobufWkt::Struct &filter_metadata_struct) override;

private:
  RouteFaultFilterConfigSharedPtr route_filter_config_;
};

} // namespace Http
} // namespace Envoy
