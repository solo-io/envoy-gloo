#pragma once

#include "envoy/server/filter_config.h"

#include "source/common/buffer/buffer_impl.h"

#include "source/extensions/filters/http/transformation/transformation_filter_config.h"
#include "source/extensions/filters/http/transformation/transformer.h"

#include "api/envoy/config/filter/http/transformation/v2/transformation_filter.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

/**
 * Translation we can be used either as a functional filter, or a non functional
 * filter.
 */
class TransformationFilter : public Http::StreamFilter,
                             Logger::Loggable<Logger::Id::filter> {
public:
  TransformationFilter(FilterConfigSharedPtr);
  ~TransformationFilter();

  // Http::FunctionalFilterBase
  void onDestroy() override;

  void onStreamComplete() override;

  // Http::StreamDecoderFilter
  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap &,
                                          bool) override;
  Http::FilterDataStatus decodeData(Buffer::Instance &, bool) override;
  Http::FilterTrailersStatus decodeTrailers(Http::RequestTrailerMap &) override;

  void setDecoderFilterCallbacks(
      Http::StreamDecoderFilterCallbacks &callbacks) override {
    decoder_callbacks_ = &callbacks;
    decoder_buffer_limit_ = callbacks.decoderBufferLimit();
  };

  // Http::StreamEncoderFilter
  Http::Filter1xxHeadersStatus
  encode1xxHeaders(Http::ResponseHeaderMap &) override {
    return Http::Filter1xxHeadersStatus::Continue;
  }
  Http::FilterHeadersStatus encodeHeaders(Http::ResponseHeaderMap &headers,
                                          bool end_stream) override;
  Http::FilterDataStatus encodeData(Buffer::Instance &data,
                                    bool end_stream) override;
  Http::FilterTrailersStatus
  encodeTrailers(Http::ResponseTrailerMap &trailers) override;
  Http::FilterMetadataStatus encodeMetadata(Http::MetadataMap &) override {
    return Http::FilterMetadataStatus::Continue;
  }

  void setEncoderFilterCallbacks(
      Http::StreamEncoderFilterCallbacks &callbacks) override {
    encoder_callbacks_ = &callbacks;
    encoder_buffer_limit_ = callbacks.encoderBufferLimit();
  };

private:
  enum class Error {
    PayloadTooLarge,
    JsonParseError,
    TemplateParseError,
    TransformationNotFound,
  };

  enum class Direction {
    Request,
    Response,
  };
  virtual void setupTransformationPair();

  bool requestActive() { return request_transformation_ != nullptr; }
  bool responseActive() { return response_transformation_ != nullptr; }
  void requestError();
  void responseError();
  void error(Error error, std::string msg = "");
  bool is_error();

  // TransformerConstSharedPtr getTransformFromRoute(Direction direction);

  void transformRequest();
  void transformResponse();
  void transformOnStreamCompletion();

  void addDecoderData(Buffer::Instance &data);
  void addEncoderData(Buffer::Instance &data);
  void
  transformSomething(Http::StreamFilterCallbacks &callbacks,
                     TransformerConstSharedPtr &transformation,
                     Http::RequestOrResponseHeaderMap &header_map,
                     Buffer::Instance &body,
                     void (TransformationFilter::*responeWithError)(),
                     void (TransformationFilter::*addData)(Buffer::Instance &));

  void resetInternalState();

  Http::StreamDecoderFilterCallbacks *decoder_callbacks_{};
  Http::StreamEncoderFilterCallbacks *encoder_callbacks_{};
  Router::RouteConstSharedPtr route_;
  const RouteFilterConfig *route_config_{};
  uint32_t decoder_buffer_limit_{};
  uint32_t encoder_buffer_limit_{};
  Http::RequestHeaderMap *request_headers_{nullptr};
  Http::ResponseHeaderMap *response_headers_{nullptr};
  Buffer::OwnedImpl request_body_{};
  Buffer::OwnedImpl response_body_{};

  TransformerConstSharedPtr request_transformation_;
  TransformerConstSharedPtr response_transformation_;
  TransformerConstSharedPtr on_stream_completion_transformation_;
  absl::optional<Error> error_;
  Http::Code error_code_;
  std::string error_messgae_;
  bool should_clear_cache_{};
  bool destroyed_{};

  FilterConfigSharedPtr filter_config_;
  // Determines whether the stream has been ended for running the filter in upstream mode.
  absl::optional<bool> latched_end_stream_;
};

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
