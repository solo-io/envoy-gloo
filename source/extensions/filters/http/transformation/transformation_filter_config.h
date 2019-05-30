#pragma once

#include <string>

#include "envoy/router/router.h"

#include "api/envoy/config/filter/http/transformation/v2/transformation_filter.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

class RouteTransformationFilterConfig
    : public Router::RouteSpecificFilterConfig {

  using ProtoConfig = envoy::api::v2::filter::http::RouteTransformations;

public:
  RouteTransformationFilterConfig(ProtoConfig proto_config)
      : proto_config_(proto_config) {}

  const envoy::api::v2::filter::http::Transformation *
  getRequestTranformation() const {
    return proto_config_.has_request_transformation()
               ? &proto_config_.request_transformation()
               : nullptr;
  }

  bool shouldClearCache() const {
    return proto_config_.clear_route_cache();
  }

  const envoy::api::v2::filter::http::Transformation *
  getResponseTranformation() const {
    return proto_config_.has_response_transformation()
               ? &proto_config_.response_transformation()
               : nullptr;
  }

private:
  ProtoConfig proto_config_;
};

typedef std::shared_ptr<const RouteTransformationFilterConfig>
    RouteTransformationFilterConfigConstSharedPtr;

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
