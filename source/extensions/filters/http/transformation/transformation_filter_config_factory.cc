#include "extensions/filters/http/transformation/transformation_filter_config_factory.h"

#include <string>

#include "envoy/registry/registry.h"

#include "common/common/macros.h"
#include "common/config/json_utility.h"
#include "common/protobuf/utility.h"

#include "extensions/filters/http/transformation/transformation_filter.h"
#include "extensions/filters/http/transformation/transformation_filter_config.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

Http::FilterFactoryCb TransformationFilterConfigFactory::createFilter(
    const std::string &, Server::Configuration::FactoryContext &) {

  return [](Http::FilterChainFactoryCallbacks &callbacks) -> void {
    auto filter = new TransformationFilter();
    callbacks.addStreamFilter(Http::StreamFilterSharedPtr{filter});
  };
}

ProtobufTypes::MessagePtr
TransformationFilterConfigFactory::createEmptyRouteConfigProto() {
  return std::make_unique<envoy::api::v2::filter::http::RouteTransformations>();
}

Router::RouteSpecificFilterConfigConstSharedPtr
TransformationFilterConfigFactory::createRouteSpecificFilterConfig(
    const Protobuf::Message &config, Server::Configuration::FactoryContext &) {
  const auto &proto_config =
      dynamic_cast<const envoy::api::v2::filter::http::RouteTransformations &>(
          config);
  return std::make_shared<const RouteTransformationFilterConfig>(proto_config);
}

/**
 * Static registration for this sample filter. @see RegisterFactory.
 */
REGISTER_FACTORY(TransformationFilterConfigFactory,
                 Server::Configuration::NamedHttpFilterConfigFactory);

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
