#include "common/http/filter/transformation_filter.h"

#include "common/config/metadata.h"
#include "common/config/transformation_well_known_names.h"
#include "common/http/filter/transformer.h"
#include "common/http/solo_filter_utility.h"
#include "common/http/utility.h"

namespace Envoy {
namespace Http {

TransformationFilter::TransformationFilter(
    TransformationFilterConfigSharedPtr config)
    : config_(config) {}

TransformationFilter::~TransformationFilter() {}

void TransformationFilter::onDestroy() {
  resetInternalState();
  stream_destroyed_ = true;
}

FilterHeadersStatus TransformationFilter::decodeHeaders(HeaderMap &header_map,
                                                        bool end_stream) {

  checkActive();

  if (!active()) {
    return FilterHeadersStatus::Continue;
  }

  header_map_ = &header_map;

  if (end_stream) {
    transform();
    return FilterHeadersStatus::Continue;
  }

  return FilterHeadersStatus::StopIteration;
}

FilterDataStatus TransformationFilter::decodeData(Buffer::Instance &data,
                                                  bool end_stream) {
  if (!active()) {
    return FilterDataStatus::Continue;
  }

  body_.move(data);
  if ((decoder_buffer_limit_ != 0) &&
      (body_.length() > decoder_buffer_limit_)) {
    Http::Utility::sendLocalReply(*callbacks_, stream_destroyed_,
                                  Http::Code::PayloadTooLarge,
                                  "payload too large");
  }

  if (end_stream) {
    transform();
    return FilterDataStatus::Continue;
  }

  return FilterDataStatus::StopIterationNoBuffer;
}

FilterTrailersStatus TransformationFilter::decodeTrailers(HeaderMap &) {
  if (active()) {
    transform();
  }
  return FilterTrailersStatus::Continue;
}

void TransformationFilter::checkActive() {
  route_ = callbacks_->route();
  if (!route_) {
    return;
  }

  const Router::RouteEntry *routeEntry = route_->routeEntry();
  if (!routeEntry) {
    return;
  }

  const ProtobufWkt::Value &value = Config::Metadata::metadataValue(
      routeEntry->metadata(),
      Config::TransformationMetadataFilters::get().TRANSFORMATION,
      Config::MetadataTransformationKeys::get().TRANSFORMATION);

  if (value.kind_case() != ProtobufWkt::Value::kStringValue) {
    return;
  }

  const auto &string_value = value.string_value();
  if (string_value.empty()) {
    return;
  }

  transformation_ = config_->getTranformation(string_value);
}

void TransformationFilter::transform() {
  Transformer transformer(*transformation_);
  transformer.transform(*header_map_, body_);
  callbacks_->addDecodedData(body_, false);
}

void TransformationFilter::resetInternalState() { body_.drain(body_.length()); }

} // namespace Http
} // namespace Envoy
