#include "source/extensions/filters/http/nats/streaming/nats_streaming_route_specific_filter_config.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Nats {
namespace Streaming {

NatsStreamingRouteSpecificFilterConfig::NatsStreamingRouteSpecificFilterConfig(
    const envoy::config::filter::http::nats::streaming::v2::
        NatsStreamingPerRoute &proto_config)
    : subject_(proto_config.subject()), cluster_id_(proto_config.cluster_id()),
      discover_prefix_(proto_config.discover_prefix()) {}

} // namespace Streaming
} // namespace Nats
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
