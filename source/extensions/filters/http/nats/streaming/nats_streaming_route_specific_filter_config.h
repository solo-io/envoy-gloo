#pragma once

#include "envoy/router/router.h"

#include "api/envoy/config/filter/http/nats/streaming/v2/nats_streaming.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Nats {
namespace Streaming {

class NatsStreamingRouteSpecificFilterConfig
    : public Router::RouteSpecificFilterConfig {
public:
  NatsStreamingRouteSpecificFilterConfig(
      const envoy::config::filter::http::nats::streaming::v2::
          NatsStreamingPerRoute &proto_config);

  const std::string &subject() const { return subject_; }
  const std::string &clusterId() const { return cluster_id_; }
  const std::string &discoverPrefix() const { return discover_prefix_; }

private:
  const std::string subject_;
  const std::string cluster_id_;
  const std::string discover_prefix_;
};

} // namespace Streaming
} // namespace Nats
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
