#pragma once

#include <string>

#include "envoy/router/router.h"

#include "extensions/filters/http/transformation/transformer.h"

#include "api/envoy/config/filter/http/transformation/v2/transformation_filter.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

class Transformation {
public:
  static TransformerSharedPtr getTransformer(
      const envoy::api::v2::filter::http::Transformation &transformation);
};

class RouteTransformationFilterConfig
    : public Router::RouteSpecificFilterConfig {

  using ProtoConfig = envoy::api::v2::filter::http::RouteTransformations;

public:
  RouteTransformationFilterConfig(ProtoConfig proto_config)
      : clear_route_cache_(proto_config.clear_route_cache()) {
    if (proto_config.has_request_transformation()) {
      request_transformation_ =
          Transformation::getTransformer(proto_config.request_transformation());
    }
    if (proto_config.has_response_transformation()) {
      response_transformation_ = Transformation::getTransformer(
          proto_config.response_transformation());
    }
  }

  TransformerConstSharedPtr getRequestTranformation() const {
    return request_transformation_;
  }

  bool shouldClearCache() const { return clear_route_cache_; }

  TransformerConstSharedPtr getResponseTranformation() const {
    return response_transformation_;
  }

private:
  TransformerConstSharedPtr request_transformation_;
  TransformerConstSharedPtr response_transformation_;
  bool clear_route_cache_{};
};

typedef std::shared_ptr<const RouteTransformationFilterConfig>
    RouteTransformationFilterConfigConstSharedPtr;

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
