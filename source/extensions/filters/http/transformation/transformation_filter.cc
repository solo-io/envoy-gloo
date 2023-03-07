#include "source/extensions/filters/http/transformation/transformation_filter.h"

#include "source/common/common/empty_string.h"
#include "source/common/common/enum_to_int.h"
#include "source/common/config/metadata.h"
#include "source/common/http/header_utility.h"
#include "source/common/http/header_map_impl.h"
#include "source/common/http/utility.h"

#include "source/extensions/filters/http/solo_well_known_names.h"
#include "source/extensions/filters/http/transformation/transformer.h"

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

  if (!hasRequestTransformation()) {
    return Http::FilterHeadersStatus::Continue;
  }

  if (end_stream || request_transformation_->passthrough_body()) {
    filter_config_->stats().request_header_transformations_.inc();
    transformRequest();
    std::cout << "after transforming request" << std::endl;
    return is_error() ? Http::FilterHeadersStatus::StopIteration
                      : Http::FilterHeadersStatus::Continue;
  }

  return Http::FilterHeadersStatus::StopIteration;
}

Http::FilterDataStatus TransformationFilter::decodeData(Buffer::Instance &data,
                                                        bool end_stream) {
  if (!hasRequestTransformation()) {
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
  if (hasRequestTransformation()) {
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

  if (!hasResponseTransformation()) {
    // this also covers the is_error() case. as is_error() == true implies
    // hasResponseTransformation() == false
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
  if (!hasResponseTransformation()) {
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
  if (hasResponseTransformation()) {
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
  active_transformer_pair = config_to_use->findTransformers(*request_headers_);

  if (active_transformer_pair != nullptr) {
    std::cout << "request_transformation" << active_transformer_pair->getRequestTranformation() << std::endl;
    std::cout << "response_transformation" << active_transformer_pair->getResponseTranformation() << std::endl;
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
  try {
    request_transformation_->transform(*request_headers_, request_body_, *decoder_callbacks_);
  } catch (const std::exception &e) {
    error(Error::TemplateParseError, e.what());
    ENVOY_STREAM_LOG(debug,
                     "failure transforming response {}",
                     *encoder_callbacks_,
                     e.what());
  }
  finalizeTransformation(*decoder_callbacks_, request_transformation_, *request_headers_, request_body_,
                     &filter_config_->stats().request_error_,
                     &TransformationFilter::requestError,
                     &TransformationFilter::addDecoderData);
  request_transformation_ = nullptr;
  if (should_clear_cache_) {
    decoder_callbacks_->downstreamCallbacks()->clearRouteCache();
  }
}

void TransformationFilter::transformResponse() {
  std::cout << "1" << std::endl;
  try {
    response_transformation_->transform(*response_headers_, response_body_, *decoder_callbacks_);
  } catch (const std::exception &e) {
    error(Error::TemplateParseError, e.what());
    ENVOY_STREAM_LOG(debug,
                     "failure transforming response {}",
                     *encoder_callbacks_,
                     e.what());
  }
  std::cout << "2" << std::endl;
  finalizeTransformation(*encoder_callbacks_, response_transformation_, *response_headers_, response_body_,
                     &filter_config_->stats().response_error_,
                     &TransformationFilter::responseError,
                     &TransformationFilter::addEncoderData);
  response_transformation_ = nullptr;
  std::cout << "9" << std::endl;
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
  std::cout << "1" << std::endl;

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
  std::cout << "2" << std::endl;

  try {
    std::cout << "!3" << std::endl;
    on_stream_completion_transformation_->transform(*response_headers_,*request_headers_, emptyBody, *encoder_callbacks_);
    std::cout << "!4" << std::endl;
  } catch (std::exception &e)  {
    error(Error::TemplateParseError, e.what());
    std::cout << "8" << std::endl;
    ENVOY_STREAM_LOG(debug,
                     "failure transforming on stream completion {}",
                     *encoder_callbacks_,
                     e.what());
  }
  finalizeTransformation(*encoder_callbacks_, on_stream_completion_transformation_, *response_headers_, emptyBody,
          &filter_config_->stats().on_stream_complete_error_,
          &TransformationFilter::responseError,
          &TransformationFilter::addEncoderData);
  on_stream_completion_transformation_ = nullptr;
  std::cout << "9" << std::endl;
}

void TransformationFilter::finalizeTransformation(
    Http::StreamFilterCallbacks &callbacks,
    TransformerConstSharedPtr transformation,
    Http::RequestOrResponseHeaderMap &header_map, Buffer::Instance &body,
    Envoy::Stats::Counter *inc_counter,
    void (TransformationFilter::*respondWithError)(),
    void (TransformationFilter::*addData)(Buffer::Instance &)) {
  // first check if an error occurred during the transformation itself
  if (is_error()) {
    inc_counter->inc();
    (this->*respondWithError)();
    return;
  }

  try {
  std::cout << "3" << std::endl;
    if (body.length() > 0) {
  std::cout << "4.1" << std::endl;
      (this->*addData)(body);
    } else if (!transformation->passthrough_body()) {
  std::cout << "4.2" << std::endl;
      // only remove content type if the request is not passthrough.
      // This means that the empty body is a result of the transformation.
      // so the content type should be removed
      header_map.removeContentType();
    }
  } catch (std::exception &e) {
  std::cout << "5" << std::endl;
    ENVOY_STREAM_LOG(debug, "failure transforming {}", callbacks, e.what());
    error(Error::TemplateParseError, e.what());
  }

  std::cout << "6" << std::endl;
  if (is_error()) {
    inc_counter->inc();
    (this->*respondWithError)();
  }
}

void TransformationFilter::requestError() {
  ASSERT(is_error());
  /* filter_config_->stats().request_error_.inc(); */
  decoder_callbacks_->sendLocalReply(error_code_, error_message_, nullptr,
                                     absl::nullopt,
                                     RcDetails::get().TransformError);
}

void TransformationFilter::responseError() {
  ASSERT(is_error());
  /* filter_config_->stats().response_error_.inc(); */
  response_headers_->setStatus(enumToInt(error_code_));
  Buffer::OwnedImpl data(error_message_);
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
    error_message_ = "payload too large";
    error_code_ = Http::Code::PayloadTooLarge;
    break;
  }
  case Error::JsonParseError: {
    error_message_ = "bad request";
    error_code_ = Http::Code::BadRequest;
    break;
  }
  case Error::TemplateParseError: {
    error_message_ = "bad request";
    error_code_ = Http::Code::BadRequest;
    break;
  }
  case Error::TransformationNotFound: {
    error_message_ = "transformation for function not found";
    error_code_ = Http::Code::NotFound;
    break;
  }
  }
  if (!msg.empty()) {
    if (error_message_.empty()) {
      error_message_ = std::move(msg);
    } else {
      error_message_ = error_message_ + ": " + msg;
    }
  }
}

bool TransformationFilter::is_error() { return error_.has_value(); }

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
