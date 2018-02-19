#include "envoy/config/filter/http/fault/v2/fault.pb.validate.h"
#include "envoy/registry/registry.h"

#include "common/config/filter_json.h"
#include "common/http/filter/fault_filter.h"

#include "server/config/http/route_fault.h"

#include "common/http/route_enabled_filter_wrapper.h"
#include "common/config/solo_well_known_names.h"

namespace Envoy {
namespace Server {
namespace Configuration {

HttpFilterFactoryCb RouteEnabledFaultFilterConfig::createFilter(
    const envoy::config::filter::http::fault::v2::HTTPFault &config,
    const std::string &stats_prefix, FactoryContext &context) {
  Http::FaultFilterConfigSharedPtr filter_config(new Http::FaultFilterConfig(
      config, context.runtime(), stats_prefix, context.scope()));
  return [filter_config](Http::FilterChainFactoryCallbacks &callbacks) -> void {
    callbacks.addStreamDecoderFilter(Http::StreamDecoderFilterSharedPtr{
        new Http::RouteEnabledFilterWrapper<Http::FaultFilter>(
            Config::SoloCommonFilterNames::get().ROUTE_FAULT, filter_config)});
  };
}

HttpFilterFactoryCb RouteEnabledFaultFilterConfig::createFilterFactory(
    const Json::Object &json_config, const std::string &stats_prefix,
    FactoryContext &context) {
  envoy::config::filter::http::fault::v2::HTTPFault proto_config;
  Config::FilterJson::translateFaultFilter(json_config, proto_config);
  return createFilter(proto_config, stats_prefix, context);
}

HttpFilterFactoryCb RouteEnabledFaultFilterConfig::createFilterFactoryFromProto(
    const Protobuf::Message &proto_config, const std::string &stats_prefix,
    FactoryContext &context) {
  return createFilter(
      MessageUtil::downcastAndValidate<
          const envoy::config::filter::http::fault::v2::HTTPFault &>(
          proto_config),
      stats_prefix, context);
}

/**
 * Static registration for the fault filter. @see RegisterFactory.
 */
static Registry::RegisterFactory<RouteEnabledFaultFilterConfig,
                                 NamedHttpFilterConfigFactory>
    register_;

} // namespace Configuration
} // namespace Server
} // namespace Envoy
