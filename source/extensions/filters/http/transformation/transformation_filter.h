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
class TransformationFilterBase : public StreamFilter {
public:
  TransformationFilterBase(TransformationFilterConfigConstSharedPtr config);
  ~TransformationFilterBase();

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

protected:
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

  virtual const envoy::api::v2::filter::http::Transformation *
  getTransformFromRouteEntry(const Router::RouteEntry *routeEntry,
                             Direction direction) PURE;

  bool requestActive() { return request_transformation_ != nullptr; }
  bool responseActive() { return response_transformation_ != nullptr; }
  void requestError();
  void responseError();
  void error(Error error, std::string msg = "");
  bool is_error();

  TransformationFilterConfigConstSharedPtr config_;

private:
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
      void (TransformationFilterBase::*responeWithError)(),
      void (TransformationFilterBase::*addData)(Buffer::Instance &));
  void transformTemplate(
      const envoy::api::v2::filter::http::TransformationTemplate &,
      HeaderMap &header_map, Buffer::Instance &body,
      void (TransformationFilterBase::*addData)(Buffer::Instance &));
  void transformBodyHeaderTransformer(
      HeaderMap &header_map, Buffer::Instance &body,
      void (TransformationFilterBase::*addData)(Buffer::Instance &));

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

class TransformationFilter : public TransformationFilterBase {
public:
  using TransformationFilterBase::TransformationFilterBase;

protected:
  const envoy::api::v2::filter::http::Transformation *
  getTransformFromRouteEntry(
      const Router::RouteEntry *routeEntry,
      TransformationFilterBase::Direction direction) override;
};

class FunctionalTransformationFilter : public TransformationFilterBase,
                                       public FunctionalFilter {
public:
  using TransformationFilterBase::TransformationFilterBase;

  // Http::FunctionalFilter
  bool retrieveFunction(const MetadataAccessor &meta_accessor) override;

protected:
  virtual void checkRequestActive() override;
  const envoy::api::v2::filter::http::Transformation *
  getTransformFromRouteEntry(
      const Router::RouteEntry *routeEntry,
      TransformationFilterBase::Direction direction) override;

private:
  absl::optional<const std::string *> current_function_{};
};

} // namespace Http
} // namespace Envoy
