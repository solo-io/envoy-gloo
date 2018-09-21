#include "extensions/filters/http/nats/streaming/nats_streaming_filter.h"

#include <algorithm>
#include <list>
#include <string>
#include <vector>

#include "envoy/http/header_map.h"
#include "envoy/nats/streaming/client.h"

#include "common/common/macros.h"
#include "common/common/utility.h"
#include "common/http/solo_filter_utility.h"

#include "extensions/filters/http/solo_well_known_names.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Nats {
namespace Streaming {

NatsStreamingFilter::NatsStreamingFilter(
    NatsStreamingFilterConfigSharedPtr config,
    Envoy::Nats::Streaming::ClientPtr nats_streaming_client)
    : config_(config), nats_streaming_client_(nats_streaming_client) {}

NatsStreamingFilter::~NatsStreamingFilter() {}

void NatsStreamingFilter::onDestroy() {

  if (in_flight_request_ != nullptr) {
    in_flight_request_->cancel();
    in_flight_request_ = nullptr;
  }
}

Http::FilterHeadersStatus
NatsStreamingFilter::decodeHeaders(Envoy::Http::HeaderMap &headers,
                                   bool end_stream) {
  UNREFERENCED_PARAMETER(headers);

  retrieveRouteSpecificFilterConfig();

  if (!isActive()) {
    return Http::FilterHeadersStatus::Continue;
  }

  if (end_stream) {
    relayToNatsStreaming();
  }

  return Http::FilterHeadersStatus::StopIteration;
}

Http::FilterDataStatus
NatsStreamingFilter::decodeData(Envoy::Buffer::Instance &data,
                                bool end_stream) {
  if (!isActive()) {
    return Http::FilterDataStatus::Continue;
  }

  body_.move(data);

  if ((decoder_buffer_limit_.has_value()) &&
      ((body_.length() + data.length()) > decoder_buffer_limit_.value())) {

    decoder_callbacks_->sendLocalReply(Http::Code::PayloadTooLarge,
                                       "nats streaming paylaod too large",
                                       nullptr);
    return Http::FilterDataStatus::StopIterationNoBuffer;
  }

  body_.move(data);

  if (end_stream) {
    relayToNatsStreaming();

    // TODO(talnordan): We need to make sure that life time of the buffer makes
    // sense.
    return Http::FilterDataStatus::StopIterationNoBuffer;
  }

  return Http::FilterDataStatus::StopIterationNoBuffer;
}

Http::FilterTrailersStatus
NatsStreamingFilter::decodeTrailers(Envoy::Http::HeaderMap &) {
  if (!isActive()) {
    return Http::FilterTrailersStatus::Continue;
  }

  relayToNatsStreaming();
  return Http::FilterTrailersStatus::StopIteration;
}

void NatsStreamingFilter::onResponse() { onCompletion(Http::Code::OK, ""); }

void NatsStreamingFilter::onFailure() {
  onCompletion(Http::Code::InternalServerError, "nats streaming filter abort",
               RequestInfo::ResponseFlag::NoHealthyUpstream);
}

void NatsStreamingFilter::onTimeout() {
  onCompletion(Http::Code::RequestTimeout, "nats streaming filter timeout",
               RequestInfo::ResponseFlag::UpstreamRequestTimeout);
}

void NatsStreamingFilter::retrieveRouteSpecificFilterConfig() {
  const std::string &name = SoloHttpFilterNames::get().NATS_STREAMING;

  // A `shared_ptr` to the result of `route()` is stored as a member in order
  // to make sure that the pointer returned by `resolvePerFilterConfig()`
  // remains valid for the current request.
  route_ = decoder_callbacks_->route();

  const auto *route_local = Http::SoloFilterUtility::resolvePerFilterConfig<
      const NatsStreamingRouteSpecificFilterConfig>(name, route_);

  if (route_local != nullptr) {
    optional_route_specific_filter_config_ = route_local;
  }
}

void NatsStreamingFilter::relayToNatsStreaming() {
  ASSERT(optional_route_specific_filter_config_.has_value(), "");
  ASSERT(!optional_route_specific_filter_config_.value()->subject().empty(),
         "");

  const std::string *cluster_name =
      Http::SoloFilterUtility::resolveClusterName(decoder_callbacks_);
  if (!cluster_name) {
    // TODO(talnordan): Consider changing the return type to `bool` and
    // returning `false`.
    return;
  }

  auto &&route_specific_filter_config =
      optional_route_specific_filter_config_.value();
  const std::string &subject = route_specific_filter_config->subject();
  const std::string &cluster_id = route_specific_filter_config->clusterId();
  const std::string &discover_prefix =
      route_specific_filter_config->discoverPrefix();

  in_flight_request_ = nats_streaming_client_->makeRequest(
      subject, cluster_id, discover_prefix, body_, *this);
}

void NatsStreamingFilter::onCompletion(Http::Code response_code,
                                       const std::string &body_text) {
  in_flight_request_ = nullptr;

  decoder_callbacks_->sendLocalReply(response_code, body_text, nullptr);
}

void NatsStreamingFilter::onCompletion(
    Http::Code response_code, const std::string &body_text,
    RequestInfo::ResponseFlag response_flag) {
  decoder_callbacks_->requestInfo().setResponseFlag(response_flag);
  onCompletion(response_code, body_text);
}

} // namespace Streaming
} // namespace Nats
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
