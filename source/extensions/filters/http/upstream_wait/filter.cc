#include "envoy/server/filter_config.h"
#include "source/extensions/filters/http/upstream_wait/filter.h"
#include "api/envoy/config/filter/http/upstream_wait/v2/upstream_wait_filter.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace UpstreamWait {

WaitingFilter::WaitingFilter() {}

WaitingFilter::~WaitingFilter() {}


void WaitingFilter::onUpstreamConnectionEstablished() {
  if (paused_iteration_) {
    decoder_callbacks_->continueDecoding();
  }
}

Http::FilterHeadersStatus
WaitingFilter::decodeHeaders(Http::RequestHeaderMap &,
                                    bool) {

  // If not an upstream filter the upstream callbacks will be missing
  if (decoder_callbacks_->upstreamCallbacks()) {
    if (!decoder_callbacks_->upstreamCallbacks()->upstream()) {
      paused_iteration_ = true;
      return Http::FilterHeadersStatus::StopAllIterationAndWatermark;
    }
  }

  return Http::FilterHeadersStatus::Continue;
}

} // namespace UpstreamWait
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
