#include "server/config/http/transformation_filter_config_factory.h"

#include <string>

#include "envoy/registry/registry.h"

#include "common/common/macros.h"
#include "common/config/json_utility.h"
#include "common/config/transformation_well_known_names.h"
#include "common/http/filter/transformation_filter.h"
#include "common/http/filter/transformation_filter_config.h"
#include "common/http/functional_stream_decoder_base.h"
#include "common/protobuf/utility.h"

#include "transformation_filter.pb.h"

namespace Envoy {
namespace Server {
namespace Configuration {

typedef Http::FunctionalFilterMixin<Http::FunctionalTransformationFilter>
    MixedTransformationFilter;

TransformationFilterConfigFactory::TransformationFilterConfigFactory()
    : FactoryBase(Config::TransformationFilterNames::get().TRANSFORMATION) {}

Http::FilterFactoryCb
TransformationFilterConfigFactory::createFilterFactoryFromProtoTyped(
    const envoy::api::v2::filter::http::Transformations &proto_config,
    const std::string &, FactoryContext &context) {

  Http::TransformationFilterConfigConstSharedPtr config =
      std::make_shared<Http::TransformationFilterConfig>(proto_config);

  return
      [&context, config](Http::FilterChainFactoryCallbacks &callbacks) -> void {
        if (config->use_routes_for_config()) {
        // TODO: once use_routes_for_config is implemented in gloo, the functional version of this  
        // filter should be removed.
          auto filter = new Http::TransformationFilter(nullptr);
          callbacks.addStreamFilter(Http::StreamFilterSharedPtr{filter});
        } else if (!config->empty()) {
          auto filter = new Http::TransformationFilter(config);
          callbacks.addStreamFilter(Http::StreamFilterSharedPtr{filter});

          auto func_filter = new MixedTransformationFilter(
              context, Config::TransformationFilterNames::get().TRANSFORMATION,
              config);
          callbacks.addStreamFilter(Http::StreamFilterSharedPtr{func_filter});
        }
      };
}

Router::RouteSpecificFilterConfigConstSharedPtr
TransformationFilterConfigFactory::createRouteSpecificFilterConfigTyped(
    const envoy::api::v2::filter::http::RouteTransformations &proto_config,
    FactoryContext &) {
  return std::make_shared<Http::RouteTransformationFilterConfig>(proto_config);
}

/**
 * Static registration for this sample filter. @see RegisterFactory.
 */
static Registry::RegisterFactory<TransformationFilterConfigFactory,
                                 Configuration::NamedHttpFilterConfigFactory>
    register_;

} // namespace Configuration
} // namespace Server
} // namespace Envoy
