#include "source/extensions/filters/http/nats/streaming/nats_streaming_filter_config_factory.h"

#include "envoy/registry/registry.h"

#include "source/common/nats/codec_impl.h"
#include "source/common/nats/streaming/client_pool.h"
#include "source/common/tcp/conn_pool_impl.h"

#include "source/extensions/filters/http/nats/streaming/nats_streaming_filter.h"
#include "source/extensions/filters/http/nats/streaming/nats_streaming_filter_config.h"
#include "source/extensions/filters/http/nats/streaming/nats_streaming_route_specific_filter_config.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Nats {
namespace Streaming {

Http::FilterFactoryCb
NatsStreamingFilterConfigFactory::createFilterFactoryFromProtoTyped(
    const envoy::config::filter::http::nats::streaming::v2::NatsStreaming
        &proto_config,
    const std::string &, Server::Configuration::FactoryContext &context) {

  NatsStreamingFilterConfigSharedPtr config =
      std::make_shared<NatsStreamingFilterConfig>(
          NatsStreamingFilterConfig(proto_config, context.serverFactoryContext().clusterManager()));

  Tcp::ConnPoolNats::ClientFactory<Envoy::Nats::Message> &client_factory =
      Tcp::ConnPoolNats::ClientFactoryImpl<Envoy::Nats::Message,
                                           Envoy::Nats::EncoderImpl,
                                           Envoy::Nats::DecoderImpl>::instance_;

  Envoy::Nats::Streaming::ClientPtr nats_streaming_client =
      std::make_shared<Envoy::Nats::Streaming::ClientPool>(
          config->cluster(), context.serverFactoryContext().clusterManager(), client_factory,
          context.serverFactoryContext().threadLocal(), context.serverFactoryContext().api().randomGenerator(), config->opTimeout());

  return [config, nats_streaming_client](
             Envoy::Http::FilterChainFactoryCallbacks &callbacks) -> void {
    auto filter = new NatsStreamingFilter(config, nats_streaming_client);
    callbacks.addStreamDecoderFilter(
        Http::StreamDecoderFilterSharedPtr{filter});
  };
}

absl::StatusOr<Router::RouteSpecificFilterConfigConstSharedPtr>
NatsStreamingFilterConfigFactory::createRouteSpecificFilterConfigTyped(
    const envoy::config::filter::http::nats::streaming::v2::
        NatsStreamingPerRoute &proto_config,
    Server::Configuration::ServerFactoryContext &,
    ProtobufMessage::ValidationVisitor &) {
  return std::make_shared<const NatsStreamingRouteSpecificFilterConfig>(
      proto_config);
}

/**
 * Static registration for this filter. @see RegisterFactory.
 */
REGISTER_FACTORY(NatsStreamingFilterConfigFactory,
                 Server::Configuration::NamedHttpFilterConfigFactory);

} // namespace Streaming
} // namespace Nats
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
