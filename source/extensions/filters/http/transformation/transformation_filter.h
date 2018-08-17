#pragma once

#include "envoy/http/metadata_accessor.h"
#include "envoy/server/filter_config.h"

#include "common/buffer/buffer_impl.h"

#include "extensions/filters/http/transformation/transformation_filter_config.h"

#include "transformation_filter.pb.h"

namespace Envoy {
namespace Http {

/**
 * Translation we can be used either as a functional filter, or a non functional
 * filter.
 */
class TransformationFilter : public StreamFilter {
public:
  TransformationFilter();
  ~TransformationFilter();

  // Http::FunctionalFilterBase
  void onDestroy() override;

  // Http::StreamDecoderFilter
  FilterHeadersStatus decodeHeaders(HeaderMap &, bool) override;
  FilterDataStatus decodeData(Buffer::Instance &, bool) override;
  FilterTrailersStatus decodeTrailers(HeaderMap &) override;

  void
  setDecoderFilterCallbacks(StreamDecoderFilterCallbacks &callbacks) override {
    decoder_callbacks_ = &callbacks;
    decoder_buffer_limit_ = callbacks.decoderBufferLimit();
  };

  // Http::StreamEncoderFilter
  FilterHeadersStatus encode100ContinueHeaders(HeaderMap &) override {
    return FilterHeadersStatus::Continue;
  }
  FilterHeadersStatus encodeHeaders(HeaderMap &headers,
                                    bool end_stream) override;
  FilterDataStatus encodeData(Buffer::Instance &data, bool end_stream) override;
  FilterTrailersStatus encodeTrailers(HeaderMap &trailers) override;

  void
  setEncoderFilterCallbacks(StreamEncoderFilterCallbacks &callbacks) override {
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

  static const std::string &directionToKey(Direction d);

  virtual void checkRequestActive();
  virtual void checkResponseActive();

  bool requestActive() { return request_transformation_ != nullptr; }
  bool responseActive() { return response_transformation_ != nullptr; }
  void requestError();
  void responseError();
  void error(Error error, std::string msg = "");
  bool is_error();

  static bool
  isPassthrough(const envoy::api::v2::filter::http::Transformation &t) {
    if (t.transformation_type_case() ==
        envoy::api::v2::filter::http::Transformation::kTransformationTemplate) {
      return t.transformation_template().has_passthrough();
    }
    return false;
  }

  const envoy::api::v2::filter::http::Transformation *
  getTransformFromRoute(Direction direction);

  void transformRequest();
  void transformResponse();

  void addDecoderData(Buffer::Instance &data);
  void addEncoderData(Buffer::Instance &data);
  void transformSomething(
      const envoy::api::v2::filter::http::Transformation **transformation,
      HeaderMap &header_map, Buffer::Instance &body,
      void (TransformationFilter::*responeWithError)(),
      void (TransformationFilter::*addData)(Buffer::Instance &));
  void transformTemplate(
      const envoy::api::v2::filter::http::TransformationTemplate &,
      HeaderMap &header_map, Buffer::Instance &body,
      void (TransformationFilter::*addData)(Buffer::Instance &));
  void transformBodyHeaderTransformer(
      HeaderMap &header_map, Buffer::Instance &body,
      void (TransformationFilter::*addData)(Buffer::Instance &));

  void resetInternalState();

  StreamDecoderFilterCallbacks *decoder_callbacks_{};
  StreamEncoderFilterCallbacks *encoder_callbacks_{};
  Router::RouteConstSharedPtr route_;
  uint32_t decoder_buffer_limit_{};
  uint32_t encoder_buffer_limit_{};
  HeaderMap *request_headers_{nullptr};
  HeaderMap *response_headers_{nullptr};
  Buffer::OwnedImpl request_body_{};
  Buffer::OwnedImpl response_body_{};

  const envoy::api::v2::filter::http::Transformation *request_transformation_{
      nullptr};
  const envoy::api::v2::filter::http::Transformation *response_transformation_{
      nullptr};
  absl::optional<Error> error_;
  Code error_code_;
  std::string error_messgae_;
};

} // namespace Http
} // namespace Envoy
