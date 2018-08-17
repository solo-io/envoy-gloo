#pragma once

#include <string>

#include "envoy/router/router.h"

#include "transformation_filter.pb.h"

namespace Envoy {
namespace Http {

class RouteTransformationFilterConfig
    : public Router::RouteSpecificFilterConfig {

  using ProtoConfig = envoy::api::v2::filter::http::RouteTransformations;

public:
  RouteTransformationFilterConfig(ProtoConfig proto_config)
      : proto_config_(proto_config) {}

  const envoy::api::v2::filter::http::Transformation &
  getRequestTranformation() const {
    return proto_config_.request_transformation();
  }
  const envoy::api::v2::filter::http::Transformation &
  getResponseTranformation() const {
    return proto_config_.response_transformation();
  }

private:
  ProtoConfig proto_config_;
};

typedef std::shared_ptr<const RouteTransformationFilterConfig>
    RouteTransformationFilterConfigConstSharedPtr;

} // namespace Http
} // namespace Envoy
