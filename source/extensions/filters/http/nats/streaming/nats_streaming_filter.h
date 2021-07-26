#pragma once

#include "include/envoy/nats/streaming/client.h"

#include "source/extensions/filters/http/nats/streaming/nats_streaming_filter_config.h"
#include "source/extensions/filters/http/nats/streaming/nats_streaming_route_specific_filter_config.h"

#include "api/envoy/config/filter/http/nats/streaming/v2/nats_streaming.pb.validate.h"
#include "api/envoy/config/filter/http/nats/streaming/v2/payload.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Nats {
namespace Streaming {

using Upstream::ClusterManager;

class NatsStreamingFilter : public Http::StreamDecoderFilter,
                            public Envoy::Nats::Streaming::PublishCallbacks {
public:
  NatsStreamingFilter(NatsStreamingFilterConfigSharedPtr config,
                      Envoy::Nats::Streaming::ClientPtr nats_streaming_client);
  ~NatsStreamingFilter();

  // Http::StreamFilterBase
  void onDestroy() override;

  // Http::StreamDecoderFilter
  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap &,
                                          bool) override;
  Http::FilterDataStatus decodeData(Buffer::Instance &, bool) override;
  Http::FilterTrailersStatus decodeTrailers(Http::RequestTrailerMap &) override;

  void setDecoderFilterCallbacks(
      Http::StreamDecoderFilterCallbacks &decoder_callbacks) override {
    decoder_callbacks_ = &decoder_callbacks;
    auto decoder_limit = decoder_callbacks.decoderBufferLimit();
    if (decoder_limit > 0) {
      decoder_buffer_limit_ = decoder_limit;
    }
  }

  // Nats::Streaming::PublishCallbacks
  virtual void onResponse() override;
  virtual void onFailure() override;
  virtual void onTimeout() override;

private:
  void retrieveRouteSpecificFilterConfig();

  inline bool isActive() {
    return optional_route_specific_filter_config_.has_value();
  }

  void relayToNatsStreaming();

  inline void onCompletion(Http::Code response_code,
                           const std::string &body_text);

  inline void onCompletion(Http::Code response_code,
                           const std::string &body_text,
                           StreamInfo::ResponseFlag response_flag);

  const NatsStreamingFilterConfigSharedPtr config_;
  Envoy::Nats::Streaming::ClientPtr nats_streaming_client_;
  Router::RouteConstSharedPtr route_;
  absl::optional<const NatsStreamingRouteSpecificFilterConfig *>
      optional_route_specific_filter_config_;
  Http::StreamDecoderFilterCallbacks *decoder_callbacks_{};
  absl::optional<uint32_t> decoder_buffer_limit_{};
  pb::Payload payload_;
  Buffer::OwnedImpl body_{};
  Envoy::Nats::Streaming::PublishRequestPtr in_flight_request_{};
};

} // namespace Streaming
} // namespace Nats
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
