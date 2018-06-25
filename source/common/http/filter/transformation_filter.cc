#include "common/http/filter/transformation_filter.h"

#include "common/common/empty_string.h"
#include "common/common/enum_to_int.h"
#include "common/config/metadata.h"
#include "common/config/transformation_well_known_names.h"
#include "common/http/filter/body_header_transformer.h"
#include "common/http/filter/transformer.h"
#include "common/http/solo_filter_utility.h"
#include "common/http/utility.h"

namespace Envoy {
namespace Http {

TransformationFilterBase::TransformationFilterBase(
    TransformationFilterConfigConstSharedPtr config)
    : config_(config) {}

TransformationFilterBase::~TransformationFilterBase() {}

void TransformationFilterBase::onDestroy() { resetInternalState(); }

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

  request_headers_ = &header_map;

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

  response_headers_ = &header_map;

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

const std::string &TransformationFilterBase::directionToKey(
    TransformationFilterBase::Direction d) {
  switch (d) {
  case TransformationFilterBase::Direction::Request:
    return Config::MetadataTransformationKeys::get().REQUEST_TRANSFORMATION;
  case TransformationFilterBase::Direction::Response:
    return Config::MetadataTransformationKeys::get().RESPONSE_TRANSFORMATION;
  default:
    NOT_REACHED;
  }
}

void TransformationFilterBase::checkRequestActive() {
  route_ = decoder_callbacks_->route();
  request_transformation_ =
      getTransformFromRoute(TransformationFilterBase::Direction::Request);
}

void FunctionalTransformationFilter::checkRequestActive() {
  TransformationFilterBase::checkRequestActive();

  if (!requestActive()) {
    error(Error::TransformationNotFound);
    requestError();
  }
}

void TransformationFilterBase::checkResponseActive() {
  route_ = encoder_callbacks_->route();
  response_transformation_ =
      getTransformFromRoute(TransformationFilterBase::Direction::Response);
}

const envoy::api::v2::filter::http::Transformation *
TransformationFilterBase::getTransformFromRoute(
    TransformationFilterBase::Direction direction) {

  if (!route_) {
    return nullptr;
  }

  const auto *config = SoloFilterUtility::resolvePerFilterConfig<
      RouteTransformationFilterConfig>(
      Config::TransformationFilterNames::get().TRANSFORMATION, route_);

  if (config != nullptr) {
    switch (direction) {
    case TransformationFilterBase::Direction::Request:
      return &config->getRequestTranformation();
    case TransformationFilterBase::Direction::Response:
      return &config->getResponseTranformation();
    default:
      NOT_REACHED;
    }
  }

  if (config_ == nullptr) {
    // this can happen in per route config mode.
    return nullptr;
  }

  const Router::RouteEntry *routeEntry = route_->routeEntry();
  if (!routeEntry) {
    return nullptr;
  }

  return getTransformFromRouteEntry(routeEntry, direction);
}

const envoy::api::v2::filter::http::Transformation *
TransformationFilter::getTransformFromRouteEntry(
    const Router::RouteEntry *routeEntry,
    TransformationFilterBase::Direction direction) {
  const ProtobufWkt::Value &value = Config::Metadata::metadataValue(
      routeEntry->metadata(),
      Config::TransformationMetadataFilters::get().TRANSFORMATION,
      directionToKey(direction));

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
    const Router::RouteEntry *routeEntry,
    TransformationFilterBase::Direction direction) {

  const ProtobufWkt::Value &value = Config::Metadata::metadataValue(
      routeEntry->metadata(),
      Config::TransformationMetadataFilters::get().TRANSFORMATION,
      directionToKey(direction));

  if (!current_function_.has_value()) {
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
  transformSomething(&request_transformation_, *request_headers_, request_body_,
                     &TransformationFilterBase::requestError,
                     &TransformationFilterBase::addDecoderData);
}

void TransformationFilterBase::transformResponse() {
  transformSomething(&response_transformation_, *response_headers_,
                     response_body_, &TransformationFilterBase::responseError,
                     &TransformationFilterBase::addEncoderData);
}

void TransformationFilterBase::addDecoderData(Buffer::Instance &data) {
  decoder_callbacks_->addDecodedData(data, false);
}

void TransformationFilterBase::addEncoderData(Buffer::Instance &data) {
  encoder_callbacks_->addEncodedData(data, false);
}

void TransformationFilterBase::transformSomething(
    const envoy::api::v2::filter::http::Transformation **transformation,
    HeaderMap &header_map, Buffer::Instance &body,
    void (TransformationFilterBase::*responeWithError)(),
    void (TransformationFilterBase::*addData)(Buffer::Instance &)) {

  switch ((*transformation)->transformation_type_case()) {
  case envoy::api::v2::filter::http::Transformation::kTransformationTemplate:
    transformTemplate((*transformation)->transformation_template(), header_map,
                      body, addData);
    break;
  case envoy::api::v2::filter::http::Transformation::kHeaderBodyTransform:
    transformBodyHeaderTransformer(header_map, body, addData);
    break;
  case envoy::api::v2::filter::http::Transformation::
      TRANSFORMATION_TYPE_NOT_SET:
    break;
  }

  *transformation = nullptr;
  if (is_error()) {
    (this->*responeWithError)();
  }
}

void TransformationFilterBase::transformTemplate(
    const envoy::api::v2::filter::http::TransformationTemplate &transformation,
    HeaderMap &header_map, Buffer::Instance &body,
    void (TransformationFilterBase::*addData)(Buffer::Instance &)) {
  try {
    Transformer transformer(transformation);
    transformer.transform(header_map, body);

    if (body.length() > 0) {
      (this->*addData)(body);
    } else {
      header_map.removeContentType();
    }
  } catch (nlohmann::json::parse_error &e) {
    // json may throw parse error
    error(Error::JsonParseError, e.what());
  } catch (std::runtime_error &e) {
    // inja may throw runtime error
    error(Error::TemplateParseError, e.what());
  }
}

void TransformationFilterBase::transformBodyHeaderTransformer(
    HeaderMap &header_map, Buffer::Instance &body,
    void (TransformationFilterBase::*addData)(Buffer::Instance &)) {
  try {
    BodyHeaderTransformer transformer;
    transformer.transform(header_map, body);

    if (body.length() > 0) {
      (this->*addData)(body);
    } else {
      header_map.removeContentType();
    }
  } catch (nlohmann::json::parse_error &e) {
    // json may throw parse error
    error(Error::JsonParseError, e.what());
  }
}

void TransformationFilterBase::requestError() {
  ASSERT(is_error());
  decoder_callbacks_->sendLocalReply(error_code_, error_messgae_, nullptr);
}

void TransformationFilterBase::responseError() {
  ASSERT(is_error());
  response_headers_->Status()->value(enumToInt(error_code_));
  Buffer::OwnedImpl data(error_messgae_);
  response_headers_->removeContentType();
  response_headers_->insertContentLength().value(data.length());
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
    error_code_ = Code::PayloadTooLarge;
    break;
  }
  case Error::JsonParseError: {
    error_messgae_ = "bad request";
    error_code_ = Code::BadRequest;
    break;
  }
  case Error::TemplateParseError: {
    error_messgae_ = "bad request";
    error_code_ = Code::BadRequest;
    break;
  }
  case Error::TransformationNotFound: {
    error_messgae_ = "transformation for function not found";
    error_code_ = Code::NotFound;
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

bool TransformationFilterBase::is_error() { return error_.has_value(); }

} // namespace Http
} // namespace Envoy
