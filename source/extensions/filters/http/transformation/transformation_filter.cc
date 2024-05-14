#include "source/extensions/filters/http/transformation/transformation_filter.h"

#include "source/common/common/empty_string.h"
#include "source/common/common/enum_to_int.h"
#include "source/common/config/metadata.h"
#include "source/common/http/header_utility.h"
#include "source/common/http/header_map_impl.h"
#include "source/common/http/utility.h"

#include "source/extensions/filters/http/solo_well_known_names.h"
#include "source/extensions/filters/http/transformation/transformer.h"
#include "source/extensions/filters/http/transformation/transformation_logger.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

struct RcDetailsValues {
  // The fault filter injected an abort for this request.
  const std::string TransformError = "transformation_filter_error";
};
typedef ConstSingleton<RcDetailsValues> RcDetails;

TransformationFilter::TransformationFilter(FilterConfigSharedPtr config)
    : filter_config_(config) {}

TransformationFilter::~TransformationFilter() {}

void TransformationFilter::onDestroy() { 
  destroyed_ = true;
  resetInternalState(); 
}

void TransformationFilter::onStreamComplete() { transformOnStreamCompletion(); }

Http::FilterHeadersStatus
TransformationFilter::decodeHeaders(Http::RequestHeaderMap &header_map,
                                    bool end_stream) {

  request_headers_ = &header_map;
  setupTransformationPair();
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
TransformationFilter::decodeTrailers(Http::RequestTrailerMap &) {
  if (requestActive()) {
    filter_config_->stats().request_body_transformations_.inc();
    transformRequest();
  }
  return is_error() ? Http::FilterTrailersStatus::StopIteration
                    : Http::FilterTrailersStatus::Continue;
}

Http::FilterHeadersStatus
TransformationFilter::encodeHeaders(Http::ResponseHeaderMap &header_map,
                                    bool end_stream) {
  response_headers_ = &header_map;

  if (!response_transformation_ && route_config_ != nullptr) {
    const TransformConfig *staged_config =
        route_config_->transformConfigForStage(filter_config_->stage());
    if (staged_config) {
      response_transformation_ = staged_config->findResponseTransform(
          *response_headers_, encoder_callbacks_->streamInfo());
    } else {
      response_transformation_ = filter_config_->findResponseTransform(
          *response_headers_, encoder_callbacks_->streamInfo());
    }
  }

  if (!responseActive()) {
    // this also covers the is_error() case. as is_error() == true implies
    // responseActive() == false
    return destroyed_ ? Http::FilterHeadersStatus::StopIteration : Http::FilterHeadersStatus::Continue;
  }
  if (end_stream || response_transformation_->passthrough_body()) {
    filter_config_->stats().response_header_transformations_.inc();
    transformResponse();
    return destroyed_ ? Http::FilterHeadersStatus::StopIteration : Http::FilterHeadersStatus::Continue;
  }

  return Http::FilterHeadersStatus::StopIteration;
}

Http::FilterDataStatus TransformationFilter::encodeData(Buffer::Instance &data,
                                                        bool end_stream) {
  if (!responseActive()) {
    return destroyed_ ? Http::FilterDataStatus::StopIterationNoBuffer : Http::FilterDataStatus::Continue;
  }

  response_body_.move(data);
  if ((encoder_buffer_limit_ != 0) &&
      (response_body_.length() > encoder_buffer_limit_)) {
    error(Error::PayloadTooLarge);
    responseError();
    return destroyed_ ? Http::FilterDataStatus::StopIterationNoBuffer : Http::FilterDataStatus::Continue;
  }

  if (end_stream) {
    filter_config_->stats().response_body_transformations_.inc();
    transformResponse();
    return destroyed_ ? Http::FilterDataStatus::StopIterationNoBuffer : Http::FilterDataStatus::Continue;
  }

  return Http::FilterDataStatus::StopIterationNoBuffer;
}

Http::FilterTrailersStatus
TransformationFilter::encodeTrailers(Http::ResponseTrailerMap &) {
  if (responseActive()) {
    filter_config_->stats().response_body_transformations_.inc();
    transformResponse();
  }
  return destroyed_ ? Http::FilterTrailersStatus::StopIteration : Http::FilterTrailersStatus::Continue;
}

// Creates pair of request and response transformation per route
void TransformationFilter::setupTransformationPair() {
  route_config_ =
      Http::Utility::resolveMostSpecificPerFilterConfig<RouteFilterConfig>(
          decoder_callbacks_);
  TransformerPairConstSharedPtr active_transformer_pair;
  // if there is a route level config present, automatically disregard
  // header_matching rules
  const TransformConfig *config_to_use = filter_config_.get();

  if (route_config_ != nullptr) {
    const TransformConfig *staged_config =
        route_config_->transformConfigForStage(filter_config_->stage());
    if (staged_config) {
      config_to_use = staged_config;
    }
  }
  active_transformer_pair = config_to_use->findTransformers(*request_headers_, decoder_callbacks_->streamInfo());

  if (active_transformer_pair != nullptr) {
    should_clear_cache_ = active_transformer_pair->shouldClearCache();
    request_transformation_ =
        active_transformer_pair->getRequestTranformation();
    response_transformation_ =
        active_transformer_pair->getResponseTranformation();
    on_stream_completion_transformation_ =
        active_transformer_pair->getOnStreamCompletionTransformation();
  }
}

void TransformationFilter::transformRequest() {
  transformSomething(*decoder_callbacks_, request_transformation_,
                     *request_headers_, request_body_,
                     &TransformationFilter::requestError,
                     &TransformationFilter::addDecoderData);
  request_transformation_.reset();
  if (should_clear_cache_) {
    decoder_callbacks_->downstreamCallbacks()->clearRouteCache();
  }
}

void TransformationFilter::transformResponse() {
  transformSomething(*encoder_callbacks_, response_transformation_,
                     *response_headers_, response_body_,
                     &TransformationFilter::responseError,
                     &TransformationFilter::addEncoderData);
  response_transformation_.reset();
}

void TransformationFilter::addDecoderData(Buffer::Instance &data) {
  decoder_callbacks_->addDecodedData(data, false);
}

void TransformationFilter::addEncoderData(Buffer::Instance &data) {
  encoder_callbacks_->addEncodedData(data, false);
}

void TransformationFilter::transformOnStreamCompletion() {
  if (on_stream_completion_transformation_ == nullptr) {
    return;
  }

  // Body isn't required for this transformer since it isn't included
  // in access logs
  Buffer::OwnedImpl emptyBody{};
  std::unique_ptr<Http::ResponseHeaderMapImpl> emptyResponseHeaderMap;

  // If response_headers_ is a nullptr (this can happpen if a client disconnects)
  // we pass in an empty response header to avoid errors within the transformer.
  if (response_headers_ == nullptr) {
    emptyResponseHeaderMap = Http::ResponseHeaderMapImpl::create();
    response_headers_ = emptyResponseHeaderMap.get();
  }

  try {
    on_stream_completion_transformation_->transform(*response_headers_,
                                                    request_headers_, 
                                                    emptyBody, 
                                                    *encoder_callbacks_);
  } catch (std::exception &e)  {
    ENVOY_STREAM_LOG(debug, 
                     "failure transforming on stream completion {}", 
                     *encoder_callbacks_, 
                     e.what());
    filter_config_->stats().on_stream_complete_error_.inc();
  }
  on_stream_completion_transformation_.reset();
}

void TransformationFilter::transformSomething(
    Http::StreamFilterCallbacks &callbacks,
    TransformerConstSharedPtr &transformation,
    Http::RequestOrResponseHeaderMap &header_map, Buffer::Instance &body,
    void (TransformationFilter::*responeWithError)(),
    void (TransformationFilter::*addData)(Buffer::Instance &)) {

  try {
    // if log_request_response_info_ is set on the transformation, log the
    // request body and request headers before transformation
    TRANSFORMATION_SENSITIVE_LOG(debug, "headers before transformation: {}", 
                          transformation, filter_config_, callbacks, header_map);
    TRANSFORMATION_SENSITIVE_LOG(debug, "body before transformation: {}", 
                          transformation, filter_config_, callbacks, body.toString());
    transformation->transform(header_map, request_headers_, body, callbacks);

    TRANSFORMATION_SENSITIVE_LOG(debug, "headers after transformation: {}", 
                          transformation, filter_config_, callbacks, header_map);
    TRANSFORMATION_SENSITIVE_LOG(debug, "body after transformation: {}", 
                          transformation, filter_config_, callbacks, body.toString());

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
  response_headers_->setStatus(enumToInt(error_code_));
  Buffer::OwnedImpl data(error_messgae_);
  response_headers_->removeContentType();
  response_headers_->setContentLength(data.length());
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
