#pragma once

#include "envoy/server/filter_config.h"
#include "api/envoy/config/filter/http/wait/v2/wait_filter.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Wait {

class WaitingFilter : public Http::StreamDecoderFilter,
                      Logger::Loggable<Logger::Id::filter>,
                      public Http::UpstreamCallbacks {
public: 
  WaitingFilter();
  ~WaitingFilter();

  // Http::FunctionalFilterBase
  void onDestroy() override {}

  // Http::StreamDecoderFilter
  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap &,
                                          bool) override;

  // Do nothing
  Http::FilterDataStatus decodeData(Buffer::Instance&, bool) override {
    return Http::FilterDataStatus::Continue;
  }

  Http::FilterTrailersStatus decodeTrailers(Http::RequestTrailerMap&) override {
    return Http::FilterTrailersStatus::Continue;
  }

  void setDecoderFilterCallbacks(
      Http::StreamDecoderFilterCallbacks &callbacks) override {
    decoder_callbacks_ = &callbacks;
    if (callbacks.upstreamCallbacks()) {
      decoder_callbacks_->upstreamCallbacks()->addUpstreamCallbacks(*this);
    }
  }

  // UpstreamCallbacks
  void onUpstreamConnectionEstablished() override;

private:
  Http::StreamDecoderFilterCallbacks *decoder_callbacks_{};
  // Determines whether the stream has been ended for running the filter in upstream mode.
  absl::optional<bool> latched_end_stream_;
};

} // namespace Wait
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
