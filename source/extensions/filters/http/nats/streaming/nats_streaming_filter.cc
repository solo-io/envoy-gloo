#include "source/extensions/filters/http/nats/streaming/nats_streaming_filter.h"

#include <algorithm>
#include <list>
#include <string>
#include <vector>

#include "envoy/http/header_map.h"
#include "include/envoy/nats/streaming/client.h"

#include "source/common/common/macros.h"
#include "source/common/common/utility.h"
#include "source/common/grpc/common.h"
#include "source/common/http/solo_filter_utility.h"
#include "source/common/http/utility.h"

#include "source/extensions/filters/http/solo_well_known_names.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Nats {
namespace Streaming {

struct RcDetailsValues {
  // The jwt_authn filter rejected the request
  const std::string PayloadTooLarge = "nats_payload_too_big";
  const std::string Completion = "nats_completion";
};
typedef ConstSingleton<RcDetailsValues> RcDetails;

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
NatsStreamingFilter::decodeHeaders(Envoy::Http::RequestHeaderMap &headers,
                                   bool end_stream) {
  retrieveRouteSpecificFilterConfig();

  if (!isActive()) {
    return Http::FilterHeadersStatus::Continue;
  }

  // Fill in the headers.
  // TODO(talnordan): Consider extracting a common utility function which
  // converts a `HeaderMap` to a Protobuf `Map`, to reduce code duplication
  // with `Filters::Common::ExtAuthz::CheckRequestUtils::setHttpRequest()`.
  auto *mutable_headers = payload_.mutable_headers();
  headers.iterate([mutable_headers](const Envoy::Http::HeaderEntry &e) {
    (*mutable_headers)[std::string(e.key().getStringView())] =
        std::string(e.value().getStringView());
    return Envoy::Http::HeaderMap::Iterate::Continue;
  });

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

    decoder_callbacks_->sendLocalReply(
        Http::Code::PayloadTooLarge, "nats streaming paylaod too large",
        nullptr, absl::nullopt, RcDetails::get().PayloadTooLarge);
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
NatsStreamingFilter::decodeTrailers(Envoy::Http::RequestTrailerMap &) {
  if (!isActive()) {
    return Http::FilterTrailersStatus::Continue;
  }

  relayToNatsStreaming();
  return Http::FilterTrailersStatus::StopIteration;
}

void NatsStreamingFilter::onResponse() { onCompletion(Http::Code::OK, ""); }

void NatsStreamingFilter::onFailure() {
  onCompletion(Http::Code::InternalServerError, "nats streaming filter abort",
               StreamInfo::ResponseFlag::NoHealthyUpstream);
}

void NatsStreamingFilter::onTimeout() {
  onCompletion(Http::Code::RequestTimeout, "nats streaming filter timeout",
               StreamInfo::ResponseFlag::UpstreamRequestTimeout);
}

void NatsStreamingFilter::retrieveRouteSpecificFilterConfig() {

  const auto *route_local = Http::Utility::resolveMostSpecificPerFilterConfig<
      const NatsStreamingRouteSpecificFilterConfig>(decoder_callbacks_);

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

  // TODO(talnordan): Consider minimizing content copying.
  payload_.set_body(body_.toString());
  std::string payload_string = payload_.SerializeAsString();
  in_flight_request_ = nats_streaming_client_->makeRequest(
      subject, cluster_id, discover_prefix, std::move(payload_string), *this);
}

void NatsStreamingFilter::onCompletion(Http::Code response_code,
                                       const std::string &body_text) {
  in_flight_request_ = nullptr;

  decoder_callbacks_->sendLocalReply(response_code, body_text, nullptr,
                                     absl::nullopt,
                                     RcDetails::get().Completion);
}

void NatsStreamingFilter::onCompletion(Http::Code response_code,
                                       const std::string &body_text,
                                       StreamInfo::ResponseFlag response_flag) {
  decoder_callbacks_->streamInfo().setResponseFlag(response_flag);
  onCompletion(response_code, body_text);
}

} // namespace Streaming
} // namespace Nats
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
