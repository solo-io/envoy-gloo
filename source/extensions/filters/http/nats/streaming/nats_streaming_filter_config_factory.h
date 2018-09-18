#pragma once

#include <string>

#include "common/config/nats_streaming_well_known_names.h"

#include "extensions/filters/http/common/factory_base.h"

#include "api/envoy/config/filter/http/nats/streaming/v2/nats_streaming.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Nats {
namespace Streaming {

/**
 * Config registration for the NATS Streaming filter.
 */
class NatsStreamingFilterConfigFactory
    : public Common::FactoryBase<
          envoy::config::filter::http::nats::streaming::v2::NatsStreaming,
          envoy::config::filter::http::nats::streaming::v2::
              NatsStreamingPerRoute> {
public:
  NatsStreamingFilterConfigFactory()
      : FactoryBase(
            Config::NatsStreamingHttpFilterNames::get().NATS_STREAMING) {}

private:
  Http::FilterFactoryCb createFilterFactoryFromProtoTyped(
      const envoy::config::filter::http::nats::streaming::v2::NatsStreaming
          &proto_config,
      const std::string &stats_prefix,
      Server::Configuration::FactoryContext &context) override;

  Router::RouteSpecificFilterConfigConstSharedPtr
  createRouteSpecificFilterConfigTyped(
      const envoy::config::filter::http::nats::streaming::v2::
          NatsStreamingPerRoute &proto_config,
      Server::Configuration::FactoryContext &context) override;
};

} // namespace Streaming
} // namespace Nats
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
