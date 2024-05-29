#include "envoy/server/filter_config.h"
#include "source/extensions/filters/http/wait/filter.h"
#include "api/envoy/config/filter/http/wait/v2/wait_filter.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Wait {

WaitingFilter::WaitingFilter() {}

WaitingFilter::~WaitingFilter() {}


void WaitingFilter::onUpstreamConnectionEstablished() {
  if (paused_iteration_) {
    decoder_callbacks_->continueDecoding();
  }
}

Http::FilterHeadersStatus
WaitingFilter::decodeHeaders(Http::RequestHeaderMap &,
                                    bool end_stream) {

  // If not an upstream filter the upstream callbacks will be missing
  if (decoder_callbacks_->upstreamCallbacks()) {
    if (!decoder_callbacks_->upstreamCallbacks()->upstream()) {
      paused_iteration_ = end_stream;
      return Http::FilterHeadersStatus::StopAllIterationAndWatermark;
    }
  }

  return Http::FilterHeadersStatus::Continue;
}

} // namespace Wait
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
