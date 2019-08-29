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
  Transformation(const envoy::api::v2::filter::http::Transformation& transformation);

  bool body_passthrough() const {
    return passthrough_body_;
  }

  const Transformer& transformer() const {
    return *transformer_;
  }

private:
  envoy::api::v2::filter::http::Transformation transformation_;
  std::unique_ptr<Transformer> transformer_;
  bool passthrough_body_{};
};

class RouteTransformationFilterConfig
    : public Router::RouteSpecificFilterConfig {

  using ProtoConfig = envoy::api::v2::filter::http::RouteTransformations;

public:
  RouteTransformationFilterConfig(ProtoConfig proto_config) : clear_route_cache_(proto_config.clear_route_cache()){
        if (proto_config.has_request_transformation()) {
          request_transformation_.emplace(proto_config.request_transformation());
        }
        if (proto_config.has_response_transformation()) {
          response_transformation_.emplace(proto_config.response_transformation());
        }
      }

  const absl::optional<Transformation>&
  getRequestTranformation() const {
    return request_transformation_;
  }

  bool shouldClearCache() const {
    return clear_route_cache_;
  }

  const absl::optional<Transformation>&
  getResponseTranformation() const {
    return response_transformation_;
  }

private:
  absl::optional<Transformation> request_transformation_;
  absl::optional<Transformation> response_transformation_;
  bool clear_route_cache_{};

};

typedef std::shared_ptr<const RouteTransformationFilterConfig>
    RouteTransformationFilterConfigConstSharedPtr;

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
