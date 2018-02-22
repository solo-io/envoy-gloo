#include "server/config/http/route_fault.h"

#include "envoy/registry/registry.h"

#include "common/config/filter_json.h"
#include "common/config/solo_well_known_names.h"
#include "common/http/filter/route_fault_filter.h"

#include "route_fault.pb.validate.h"

namespace Envoy {
namespace Server {
namespace Configuration {

HttpFilterFactoryCb RouteFaultFilterConfig::createFilter(
    const envoy::api::v2::filter::http::RouteFault &config,
    const std::string &stats_prefix, FactoryContext &context) {
  Http::FaultFilterConfigSharedPtr filter_config(new Http::FaultFilterConfig(
      config.http_fault(), context.runtime(), stats_prefix, context.scope()));

  Http::RouteFaultFilterConfigSharedPtr route_filter_config(
      new Http::RouteFaultFilterConfig(config));

  return [route_filter_config,
          filter_config](Http::FilterChainFactoryCallbacks &callbacks) -> void {
    callbacks.addStreamDecoderFilter(Http::StreamDecoderFilterSharedPtr{
        new Http::RouteFaultFilter(route_filter_config, filter_config)});
  };
}

HttpFilterFactoryCb RouteFaultFilterConfig::createFilterFactory(
    const Json::Object &, const std::string &, FactoryContext &) {
  NOT_IMPLEMENTED;
}

HttpFilterFactoryCb RouteFaultFilterConfig::createFilterFactoryFromProto(
    const Protobuf::Message &proto_config, const std::string &stats_prefix,
    FactoryContext &context) {
  return createFilter(
      MessageUtil::downcastAndValidate<
          const envoy::api::v2::filter::http::RouteFault &>(proto_config),
      stats_prefix, context);
}

/**
 * Static registration for the fault filter. @see RegisterFactory.
 */
static Registry::RegisterFactory<RouteFaultFilterConfig,
                                 NamedHttpFilterConfigFactory>
    register_;

} // namespace Configuration
} // namespace Server
} // namespace Envoy
