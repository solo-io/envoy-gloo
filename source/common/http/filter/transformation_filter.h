#pragma once

#include "envoy/http/metadata_accessor.h"
#include "envoy/server/filter_config.h"

#include "common/buffer/buffer_impl.h"
#include "common/http/filter/transformation_filter_config.h"

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
    return Http::FilterHeadersStatus::Continue;
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

  virtual void checkRequestActive();
  virtual void checkResponseActive();

  virtual const envoy::api::v2::filter::http::Transformation *
  getTransformFromRouteEntry(const Router::RouteEntry *routeEntry,
                             const std::string &key) PURE;

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
    return t.transformation_template().has_passthrough();
  }

  const envoy::api::v2::filter::http::Transformation *
  getTransformFromRoute(const Router::RouteConstSharedPtr &route,
                        const std::string &key);

  void transformRequest();
  void transformResponse();
  void resetInternalState();

  StreamDecoderFilterCallbacks *decoder_callbacks_{};
  StreamEncoderFilterCallbacks *encoder_callbacks_{};
  bool stream_destroyed_{};
  uint32_t decoder_buffer_limit_{};
  uint32_t encoder_buffer_limit_{};
  HeaderMap *header_map_{nullptr};
  Buffer::OwnedImpl request_body_{};
  Buffer::OwnedImpl response_body_{};

  const envoy::api::v2::filter::http::Transformation *request_transformation_{
      nullptr};
  const envoy::api::v2::filter::http::Transformation *response_transformation_{
      nullptr};
  Optional<Error> error_;
  Http::Code error_code_;
  std::string error_messgae_;
};

class TransformationFilter : public TransformationFilterBase {
public:
  using TransformationFilterBase::TransformationFilterBase;

protected:
  const envoy::api::v2::filter::http::Transformation *
  getTransformFromRouteEntry(const Router::RouteEntry *routeEntry,
                             const std::string &key) override;
};

class FunctionalTransformationFilter : public TransformationFilterBase,
                                       public FunctionalFilter {
public:
  using TransformationFilterBase::TransformationFilterBase;

  // Http::FunctionalFilter
  bool retrieveFunction(const MetadataAccessor &meta_accessor) override;

protected:
  virtual void checkRequestActive();
  const envoy::api::v2::filter::http::Transformation *
  getTransformFromRouteEntry(const Router::RouteEntry *routeEntry,
                             const std::string &key) override;

private:
  Optional<const std::string *> current_function_{};
};

} // namespace Http
} // namespace Envoy
