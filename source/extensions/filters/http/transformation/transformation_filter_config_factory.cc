#include "extensions/filters/http/transformation/transformation_filter_config_factory.h"

#include <string>

#include "envoy/registry/registry.h"

#include "common/common/macros.h"
#include "common/protobuf/utility.h"

#include "extensions/filters/http/transformation/transformation_filter.h"
#include "extensions/filters/http/transformation/transformation_filter_config.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

Http::FilterFactoryCb
TransformationFilterConfigFactory::createFilterFactoryFromProtoTyped(
    const TransformationConfigProto &proto_config,
    const std::string &stats_prefix,
    Server::Configuration::FactoryContext &context) {

  FilterConfigSharedPtr config = std::make_shared<TransformationFilterConfig>(
      proto_config, stats_prefix, context);

  return [config](Http::FilterChainFactoryCallbacks &callbacks) -> void {
    auto filter = new TransformationFilter(config);
    callbacks.addStreamFilter(Http::StreamFilterSharedPtr{filter});
  };
}

Router::RouteSpecificFilterConfigConstSharedPtr
TransformationFilterConfigFactory::createRouteSpecificFilterConfigTyped(
    const RouteTransformationConfigProto &proto_config,
    Server::Configuration::ServerFactoryContext &context,
    ProtobufMessage::ValidationVisitor &) {
  return std::make_shared<const RouteTransformationFilterConfig>(proto_config, context);
}

/**
 * Static registration for this filter. @see RegisterFactory.
 */
REGISTER_FACTORY(TransformationFilterConfigFactory,
                 Server::Configuration::NamedHttpFilterConfigFactory);

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
