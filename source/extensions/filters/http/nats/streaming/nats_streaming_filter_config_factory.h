#pragma once

#include <string>

#include "extensions/filters/http/common/factory_base.h"
#include "extensions/filters/http/solo_well_known_names.h"

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
      : FactoryBase(SoloHttpFilterNames::get().NatsStreaming) {}

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
      Server::Configuration::ServerFactoryContext &context,
      ProtobufMessage::ValidationVisitor &) override;
};

} // namespace Streaming
} // namespace Nats
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
