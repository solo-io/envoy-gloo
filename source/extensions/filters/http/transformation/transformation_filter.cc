#include "extensions/filters/http/transformation/transformation_filter.h"

#include "common/common/empty_string.h"
#include "common/common/enum_to_int.h"
#include "common/config/metadata.h"
#include "common/http/utility.h"
#include "common/http/header_utility.h"

#include "extensions/filters/http/solo_well_known_names.h"
#include "extensions/filters/http/transformation/transformer.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

struct RcDetailsValues {
  // The fault filter injected an abort for this request.
  const std::string TransformError = "transformation_filter_error";
};
typedef ConstSingleton<RcDetailsValues> RcDetails;

TransformationFilter::TransformationFilter(FilterConfigSharedPtr config) : filter_config_(config) {}

TransformationFilter::~TransformationFilter() {}

void TransformationFilter::onDestroy() { resetInternalState(); }

Http::FilterHeadersStatus
TransformationFilter::decodeHeaders(Http::HeaderMap &header_map,
                                    bool end_stream) {

  request_headers_ = &header_map;

  checkRequestActive();

  if (is_error()) {
    return Http::FilterHeadersStatus::StopIteration;
  }

  if (!requestActive()) {
    return Http::FilterHeadersStatus::Continue;
  }

  if (end_stream || request_transformation_->passthrough_body()) {
    filter_config_->stats().request_header_transformations_.inc();
    transformRequest();

    return is_error() ? Http::FilterHeadersStatus::StopIteration
                      : Http::FilterHeadersStatus::Continue;
  }

  return Http::FilterHeadersStatus::StopIteration;
}

Http::FilterDataStatus TransformationFilter::decodeData(Buffer::Instance &data,
                                                        bool end_stream) {
  if (!requestActive()) {
    return Http::FilterDataStatus::Continue;
  }

  request_body_.move(data);
  if ((decoder_buffer_limit_ != 0) &&
      (request_body_.length() > decoder_buffer_limit_)) {
    error(Error::PayloadTooLarge);
    requestError();
    return Http::FilterDataStatus::StopIterationNoBuffer;
  }

  if (end_stream) {
    filter_config_->stats().request_body_transformations_.inc();
    transformRequest();
    return is_error() ? Http::FilterDataStatus::StopIterationNoBuffer
                      : Http::FilterDataStatus::Continue;
  }

  return Http::FilterDataStatus::StopIterationNoBuffer;
}

Http::FilterTrailersStatus
TransformationFilter::decodeTrailers(Http::HeaderMap &) {
  if (requestActive()) {
    filter_config_->stats().request_body_transformations_.inc();
    transformRequest();
  }
  return is_error() ? Http::FilterTrailersStatus::StopIteration
                    : Http::FilterTrailersStatus::Continue;
}

Http::FilterHeadersStatus
TransformationFilter::encodeHeaders(Http::HeaderMap &header_map,
                                    bool end_stream) {

  response_headers_ = &header_map;

  checkResponseActive();

  if (!responseActive()) {
    // this also covers the is_error() case. as is_error() == true implies
    // responseActive() == false
    return Http::FilterHeadersStatus::Continue;
  }
  if (end_stream || response_transformation_->passthrough_body()) {
    filter_config_->stats().response_header_transformations_.inc();
    transformResponse();
    return Http::FilterHeadersStatus::Continue;
  }

  return Http::FilterHeadersStatus::StopIteration;
}

Http::FilterDataStatus TransformationFilter::encodeData(Buffer::Instance &data,
                                                        bool end_stream) {
  if (!responseActive()) {
    return Http::FilterDataStatus::Continue;
  }

  response_body_.move(data);
  if ((encoder_buffer_limit_ != 0) &&
      (response_body_.length() > encoder_buffer_limit_)) {
    error(Error::PayloadTooLarge);
    responseError();
    return Http::FilterDataStatus::Continue;
  }

  if (end_stream) {
    filter_config_->stats().response_body_transformations_.inc();
    transformResponse();
    return Http::FilterDataStatus::Continue;
  }

  return Http::FilterDataStatus::StopIterationNoBuffer;
}

Http::FilterTrailersStatus
TransformationFilter::encodeTrailers(Http::HeaderMap &) {
  if (responseActive()) {
    filter_config_->stats().response_body_transformations_.inc();
    transformResponse();
  }
  return Http::FilterTrailersStatus::Continue;
}

void TransformationFilter::checkRequestActive() {
  route_ = decoder_callbacks_->route();
  request_transformation_ =
      getTransformFromRoute(TransformationFilter::Direction::Request);
}

void TransformationFilter::checkResponseActive() {
  response_transformation_ =
      getTransformFromRoute(TransformationFilter::Direction::Response);
}

TransformerConstSharedPtr TransformationFilter::getTransformFromRoute(
    TransformationFilter::Direction direction) {

  if (!route_) {
    return nullptr;
  }

  const auto *route_config = Http::Utility::resolveMostSpecificPerFilterConfig<
      RouteTransformationFilterConfig>(filter_config_->name(), route_);

  // if there is a route level config present, automatically disregard header_matching rules
  if (route_config != nullptr) {
    has_route_level_config_ = true;
  }
  
  switch (direction) {
    case TransformationFilter::Direction::Request: {
      should_clear_cache_ = filter_config_->shouldClearCache();
      if (route_config != nullptr && route_config->getRequestTranformation() != nullptr) {
        should_clear_cache_ = route_config->shouldClearCache();
        return route_config->getRequestTranformation();
      } else {
        return filter_config_->getRequestTranformation();
      }
    }
    case TransformationFilter::Direction::Response: {
      if (route_config != nullptr && route_config->getResponseTranformation() != nullptr) {
        return route_config->getResponseTranformation();
      } else {
        return filter_config_->getResponseTranformation();
      }
    }
  }

  return nullptr;
}

void TransformationFilter::transformRequest() {
  transformSomething(*decoder_callbacks_, request_transformation_,
                     *request_headers_, request_body_,
                     &TransformationFilter::requestError,
                     &TransformationFilter::addDecoderData);
  if (should_clear_cache_) {
    decoder_callbacks_->clearRouteCache();
  }
}

void TransformationFilter::transformResponse() {
  transformSomething(*encoder_callbacks_, response_transformation_,
                     *response_headers_, response_body_,
                     &TransformationFilter::responseError,
                     &TransformationFilter::addEncoderData);
}

void TransformationFilter::addDecoderData(Buffer::Instance &data) {
  decoder_callbacks_->addDecodedData(data, false);
}

void TransformationFilter::addEncoderData(Buffer::Instance &data) {
  encoder_callbacks_->addEncodedData(data, false);
}

void TransformationFilter::transformSomething(
    Http::StreamFilterCallbacks &callbacks,
    TransformerConstSharedPtr &transformation, Http::HeaderMap &header_map,
    Buffer::Instance &body, void (TransformationFilter::*responeWithError)(),
    void (TransformationFilter::*addData)(Buffer::Instance &)) {

  // if there is no route level config, and there are header matchers, check them
  if (!has_route_level_config_ && !filter_config_->matchHeaders(*request_headers_)) {
    ENVOY_STREAM_LOG(debug, "found no header match, skipping transformation", callbacks);
    filter_config_->stats().transformations_skipped_.inc();
    // necessary as the body has been moved out of the normal buffer and into the transformation one.
    (this->*addData)(body);
    return;
  }

  try {
    transformation->transform(header_map, body, callbacks);

    if (body.length() > 0) {
      (this->*addData)(body);
    } else if (!transformation->passthrough_body()) {
      // only remove content type if the request is not passthrough.
      // This means that the empty body is a result of the transformation.
      // so the content type should be removed
      header_map.removeContentType();
    }
  } catch (std::exception &e) {
    ENVOY_STREAM_LOG(debug, "failure transforming {}", callbacks, e.what());
    error(Error::TemplateParseError, e.what());
  }

  transformation = nullptr;
  if (is_error()) {
    (this->*responeWithError)();
  }
}

void TransformationFilter::requestError() {
  ASSERT(is_error());
  filter_config_->stats().request_error_.inc();
  decoder_callbacks_->sendLocalReply(error_code_, error_messgae_, nullptr,
                                     absl::nullopt,
                                     RcDetails::get().TransformError);
}

void TransformationFilter::responseError() {
  ASSERT(is_error());
  filter_config_->stats().response_error_.inc();
  response_headers_->Status()->value(enumToInt(error_code_));
  Buffer::OwnedImpl data(error_messgae_);
  response_headers_->removeContentType();
  response_headers_->insertContentLength().value(data.length());
  encoder_callbacks_->addEncodedData(data, false);
}

void TransformationFilter::resetInternalState() {
  request_body_.drain(request_body_.length());
  response_body_.drain(response_body_.length());
}

void TransformationFilter::error(Error error, std::string msg) {
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

bool TransformationFilter::is_error() { return error_.has_value(); }

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
