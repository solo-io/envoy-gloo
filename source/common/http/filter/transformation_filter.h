#pragma once

#include "common/buffer/buffer_impl.h"
#include "envoy/server/filter_config.h"

#include "common/http/filter/transformation_filter_config.h"

#include "transformation_filter.pb.h"

namespace Envoy {
namespace Http {

class TransformationFilter : public StreamDecoderFilter {
public:
  TransformationFilter(TransformationFilterConfigSharedPtr config);
  ~TransformationFilter();

  // Http::FunctionalFilterBase
  FilterHeadersStatus decodeHeaders(HeaderMap &, bool) override;
  FilterDataStatus decodeData(Buffer::Instance &, bool) override;
  FilterTrailersStatus decodeTrailers(HeaderMap &) override;

  void onDestroy() override;
  void
  setDecoderFilterCallbacks(StreamDecoderFilterCallbacks &callbacks) override {
    callbacks_ = &callbacks;
     decoder_buffer_limit_ = callbacks.decoderBufferLimit();
  };

private:

void checkActive();
bool active() {return transformation_ != nullptr;}
void resetInternalState();
void transform();

  TransformationFilterConfigSharedPtr config_;
  StreamDecoderFilterCallbacks *callbacks_{};
  bool stream_destroyed_{};  
  uint32_t decoder_buffer_limit_{};
  HeaderMap *header_map_{nullptr};
  Buffer::OwnedImpl body_{};
  
  Router::RouteConstSharedPtr route_{};
  const envoy::api::v2::filter::http::Transformation * transformation_{nullptr};
};

} // namespace Http
} // namespace Envoy
