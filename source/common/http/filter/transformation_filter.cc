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

    return is_error() ? FilterHeadersStatus::StopIteration
                      : FilterHeadersStatus::Continue;
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
    error(Error::PayloadTooLarge);
    return FilterDataStatus::StopIterationNoBuffer;
  }

  if (end_stream) {
    transform();
    return is_error() ? FilterDataStatus::StopIterationNoBuffer
                      : FilterDataStatus::Continue;
  }

  return FilterDataStatus::StopIterationNoBuffer;
}

FilterTrailersStatus TransformationFilter::decodeTrailers(HeaderMap &) {
  if (active()) {
    transform();
  }
  return is_error() ? FilterTrailersStatus::StopIteration
                    : FilterTrailersStatus::Continue;
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
  try {
    Transformer transformer(*transformation_, config_->advanced_templates());
    transformer.transform(*header_map_, body_);
    callbacks_->addDecodedData(body_, false);
  } catch (nlohmann::json::parse_error &e) {
    // json may throw parse error
    error(Error::JsonParseError);
  } catch (std::runtime_error &e) {
    // inja may throw runtime error
    error(Error::TemplateParseError);
  }
}

void TransformationFilter::resetInternalState() { body_.drain(body_.length()); }

void TransformationFilter::error(Error error) {
  error_ = error;
  resetInternalState();
  const char *msg;
  Http::Code code;
  switch (error) {
  case Error::PayloadTooLarge: {
    msg = "payload too large";
    code = Http::Code::PayloadTooLarge;
    break;
  }
  case Error::JsonParseError: {
    msg = "bad request";
    code = Http::Code::BadRequest;
    break;
  }
  case Error::TemplateParseError: {
    msg = "bad request";
    code = Http::Code::BadRequest;
    break;
  }
  }
  Utility::sendLocalReply(*callbacks_, stream_destroyed_, code, msg);
}

bool TransformationFilter::is_error() { return error_.valid(); }

} // namespace Http
} // namespace Envoy
