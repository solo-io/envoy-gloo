#include "source/extensions/filters/http/transformation/transformation_filter_config_factory.h"

#include <string>

#include "envoy/registry/registry.h"

#include "source/common/common/macros.h"
#include "source/common/protobuf/utility.h"

#include "source/extensions/filters/http/transformation/transformation_filter.h"
#include "source/extensions/filters/http/transformation/transformation_filter_config.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

absl::StatusOr<Http::FilterFactoryCb>
TransformationFilterConfigFactory::createFilterFactoryFromProtoTyped(
    const TransformationConfigProto &proto_config,
    const std::string &stats_prefix, DualInfo,
    Server::Configuration::ServerFactoryContext &context) {

  FilterConfigSharedPtr config = std::make_shared<TransformationFilterConfig>(
      proto_config, stats_prefix, context);

  return [config](Http::FilterChainFactoryCallbacks &callbacks) -> void {
    auto filter = new TransformationFilter(config);
    callbacks.addStreamFilter(Http::StreamFilterSharedPtr{filter});
  };
}

absl::StatusOr<Router::RouteSpecificFilterConfigConstSharedPtr>
TransformationFilterConfigFactory::createRouteSpecificFilterConfigTyped(
    const RouteTransformationConfigProto &proto_config,
    Server::Configuration::ServerFactoryContext &context,
    ProtobufMessage::ValidationVisitor &) {
  return std::make_shared<const RouteTransformationFilterConfig>(proto_config, context);
}

using UpstreamTransformationFilterConfigFactory = TransformationFilterConfigFactory;

/**
 * Static registration for this filter. @see RegisterFactory.
 */
REGISTER_FACTORY(TransformationFilterConfigFactory,
                 Server::Configuration::NamedHttpFilterConfigFactory);

REGISTER_FACTORY(UpstreamTransformationFilterConfigFactory,
                 Server::Configuration::UpstreamHttpFilterConfigFactory);

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
