#include "common/http/filter/transformation_filter.h"

#include "common/common/enum_to_int.h"
#include "common/config/metadata.h"
#include "common/config/transformation_well_known_names.h"
#include "common/http/filter/transformer.h"
#include "common/http/solo_filter_utility.h"
#include "common/http/utility.h"

namespace Envoy {
namespace Http {

TransformationFilterBase::TransformationFilterBase(
    TransformationFilterConfigConstSharedPtr config)
    : config_(config) {}

TransformationFilterBase::~TransformationFilterBase() {}

void TransformationFilterBase::onDestroy() {
  resetInternalState();
  stream_destroyed_ = true;
}

bool FunctionalTransformationFilter::retrieveFunction(
    const MetadataAccessor &meta_accessor) {
  current_function_ = meta_accessor.getFunctionName();
  return true;
}

FilterHeadersStatus
TransformationFilterBase::decodeHeaders(HeaderMap &header_map,
                                        bool end_stream) {

  checkRequestActive();

  if (is_error()) {
    return FilterHeadersStatus::StopIteration;
  }

  if (!requestActive()) {
    return FilterHeadersStatus::Continue;
  }

  header_map_ = &header_map;

  if (end_stream || isPassthrough(*request_transformation_)) {
    transformRequest();

    return is_error() ? FilterHeadersStatus::StopIteration
                      : FilterHeadersStatus::Continue;
  }

  return FilterHeadersStatus::StopIteration;
}

FilterDataStatus TransformationFilterBase::decodeData(Buffer::Instance &data,
                                                      bool end_stream) {
  if (!requestActive()) {
    return FilterDataStatus::Continue;
  }

  request_body_.move(data);
  if ((decoder_buffer_limit_ != 0) &&
      (request_body_.length() > decoder_buffer_limit_)) {
    error(Error::PayloadTooLarge);
    requestError();
    return FilterDataStatus::StopIterationNoBuffer;
  }

  if (end_stream) {
    transformRequest();
    return is_error() ? FilterDataStatus::StopIterationNoBuffer
                      : FilterDataStatus::Continue;
  }

  return FilterDataStatus::StopIterationNoBuffer;
}

FilterTrailersStatus TransformationFilterBase::decodeTrailers(HeaderMap &) {
  if (requestActive()) {
    transformRequest();
  }
  return is_error() ? FilterTrailersStatus::StopIteration
                    : FilterTrailersStatus::Continue;
}

FilterHeadersStatus
TransformationFilterBase::encodeHeaders(HeaderMap &header_map,
                                        bool end_stream) {

  checkResponseActive();

  if (!responseActive()) {
    // this also covers the is_error() case. as is_error() == true implies
    // responseActive() == false
    return FilterHeadersStatus::Continue;
  }

  header_map_ = &header_map;

  if (end_stream || isPassthrough(*response_transformation_)) {
    transformResponse();
    return FilterHeadersStatus::Continue;
  }

  return FilterHeadersStatus::StopIteration;
}

FilterDataStatus TransformationFilterBase::encodeData(Buffer::Instance &data,
                                                      bool end_stream) {
  if (!responseActive()) {
    return FilterDataStatus::Continue;
  }

  response_body_.move(data);
  if ((encoder_buffer_limit_ != 0) &&
      (response_body_.length() > encoder_buffer_limit_)) {
    error(Error::PayloadTooLarge);
    responseError();
    return FilterDataStatus::Continue;
  }

  if (end_stream) {
    transformResponse();
    return FilterDataStatus::Continue;
  }

  return FilterDataStatus::StopIterationNoBuffer;
}

FilterTrailersStatus TransformationFilterBase::encodeTrailers(HeaderMap &) {
  if (responseActive()) {
    transformResponse();
  }
  return FilterTrailersStatus::Continue;
}

void TransformationFilterBase::checkRequestActive() {
  request_transformation_ = getTransformFromRoute(
      decoder_callbacks_->route(),
      Config::MetadataTransformationKeys::get().REQUEST_TRANSFORMATION);
}

void FunctionalTransformationFilter::checkRequestActive() {
  TransformationFilterBase::checkRequestActive();

  if (!requestActive()) {
    error(Error::TransformationNotFound);
    requestError();
  }
}

void TransformationFilterBase::checkResponseActive() {
  response_transformation_ = getTransformFromRoute(
      encoder_callbacks_->route(),
      Config::MetadataTransformationKeys::get().RESPONSE_TRANSFORMATION);
}

const envoy::api::v2::filter::http::Transformation *
TransformationFilterBase::getTransformFromRoute(
    const Router::RouteConstSharedPtr &route, const std::string &key) {

  if (!route) {
    return nullptr;
  }

  const Router::RouteEntry *routeEntry = route->routeEntry();
  if (!routeEntry) {
    return nullptr;
  }

  return getTransformFromRouteEntry(routeEntry, key);
}

const envoy::api::v2::filter::http::Transformation *
TransformationFilter::getTransformFromRouteEntry(
    const Router::RouteEntry *routeEntry, const std::string &key) {
  const ProtobufWkt::Value &value = Config::Metadata::metadataValue(
      routeEntry->metadata(),
      Config::TransformationMetadataFilters::get().TRANSFORMATION, key);

  // if we are not in functional mode, we expect a string:
  if (value.kind_case() != ProtobufWkt::Value::kStringValue) {
    return nullptr;
  }
  const auto &string_value = value.string_value();
  if (string_value.empty()) {
    return nullptr;
  }

  return config_->getTranformation(string_value);
}

const envoy::api::v2::filter::http::Transformation *
FunctionalTransformationFilter::getTransformFromRouteEntry(
    const Router::RouteEntry *routeEntry, const std::string &key) {

  const ProtobufWkt::Value &value = Config::Metadata::metadataValue(
      routeEntry->metadata(),
      Config::TransformationMetadataFilters::get().TRANSFORMATION, key);

  if (!current_function_.valid()) {
    return nullptr;
  }

  if (value.kind_case() != ProtobufWkt::Value::kStructValue) {
    return nullptr;
  }

  // ok we have a struct; this means that we need to retreive the function
  // from the route and get the function that way

  const auto &cluster_struct_value = value.struct_value();

  const auto &cluster_fields = cluster_struct_value.fields();

  const auto cluster_it = cluster_fields.find(routeEntry->clusterName());
  if (cluster_it == cluster_fields.end()) {
    return nullptr;
  }

  const auto &functions_value = cluster_it->second;

  if (functions_value.kind_case() != ProtobufWkt::Value::kStructValue) {
    return nullptr;
  }

  const auto &functions_fields = functions_value.struct_value().fields();

  const auto functions_it = functions_fields.find(*current_function_.value());
  if (functions_it == functions_fields.end()) {
    return nullptr;
  }

  const auto &transformation_value = functions_it->second;

  if (transformation_value.kind_case() != ProtobufWkt::Value::kStringValue) {
    return nullptr;
  }
  const auto &string_value = transformation_value.string_value();

  return config_->getTranformation(string_value);
}

void TransformationFilterBase::transformRequest() {
  try {
    // 'move' the transformation
    Transformer transformer(*request_transformation_,
                            config_->advanced_templates());
    request_transformation_ = nullptr;
    transformer.transform(*header_map_, request_body_);
    if (request_body_.length() > 0) {
      decoder_callbacks_->addDecodedData(request_body_, false);
    } else {
      header_map_->removeContentType();
    }

  } catch (nlohmann::json::parse_error &e) {
    // json may throw parse error
    error(Error::JsonParseError, e.what());
  } catch (std::runtime_error &e) {
    // inja may throw runtime error
    error(Error::TemplateParseError, e.what());
  }

  if (is_error()) {
    requestError();
  }
}

void TransformationFilterBase::transformResponse() {
  try {
    Transformer transformer(*response_transformation_,
                            config_->advanced_templates());
    response_transformation_ = nullptr;
    transformer.transform(*header_map_, response_body_);

    if (response_body_.length() > 0) {
      encoder_callbacks_->addEncodedData(response_body_, false);
    } else {
      header_map_->removeContentType();
    }
  } catch (nlohmann::json::parse_error &e) {
    // json may throw parse error
    error(Error::JsonParseError, e.what());
  } catch (std::runtime_error &e) {
    // inja may throw runtime error
    error(Error::TemplateParseError, e.what());
  }

  if (is_error()) {
    responseError();
  }
}

void TransformationFilterBase::requestError() {
  ASSERT(is_error());
  Utility::sendLocalReply(*decoder_callbacks_, stream_destroyed_, error_code_,
                          error_messgae_);
}

void TransformationFilterBase::responseError() {
  ASSERT(is_error());
  header_map_->Status()->value(enumToInt(error_code_));
  Buffer::OwnedImpl data(error_messgae_);
  header_map_->removeContentType();
  header_map_->insertContentLength().value(data.length());
  encoder_callbacks_->addEncodedData(data, false);
}

void TransformationFilterBase::resetInternalState() {
  request_body_.drain(request_body_.length());
  response_body_.drain(response_body_.length());
}

void TransformationFilterBase::error(Error error, std::string msg) {
  error_ = error;
  resetInternalState();
  switch (error) {
  case Error::PayloadTooLarge: {
    error_messgae_ = "payload too large";
    error_code_ = Http::Code::PayloadTooLarge;
    break;
  }
  case Error::JsonParseError: {
    error_messgae_ = "bad request";
    error_code_ = Http::Code::BadRequest;
    break;
  }
  case Error::TemplateParseError: {
    error_messgae_ = "bad request";
    error_code_ = Http::Code::BadRequest;
    break;
  }
  case Error::TransformationNotFound: {
    error_messgae_ = "transformation for function not found";
    error_code_ = Http::Code::NotFound;
    break;
  }
  }
  if (!msg.empty()) {
    if (error_messgae_.empty()) {
      error_messgae_ = std::move(msg);
    } else {
      error_messgae_ = error_messgae_ + ": " + msg;
    }
  }
}

bool TransformationFilterBase::is_error() { return error_.valid(); }

} // namespace Http
} // namespace Envoy
