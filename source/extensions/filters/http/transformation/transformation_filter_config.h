#pragma once

#include <string>

#include "envoy/router/router.h"

#include "extensions/filters/http/transformation/transformer.h"
#include "extensions/filters/http/solo_well_known_names.h"

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

using TransformationConfigProto = envoy::api::v2::filter::http::RouteTransformations;
using RouteTransformationConfigProto = envoy::api::v2::filter::http::RouteTransformations;

class TransformationFilterConfig : public FilterConfig {
public:
  TransformationFilterConfig(const TransformationConfigProto &proto_config, const std::string& prefix, Stats::Scope& scope)
      : FilterConfig(prefix, scope), clear_route_cache_(proto_config.clear_route_cache()) {
    if (proto_config.has_request_transformation()) {
      request_transformation_ =
          Transformation::getTransformer(proto_config.request_transformation());
    }
    if (proto_config.has_response_transformation()) {
      response_transformation_ = Transformation::getTransformer(
          proto_config.response_transformation());
    }
  }

  std::string name() const override {
    return SoloHttpFilterNames::get().Transformation;
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


class RouteTransformationFilterConfig : public RouteFilterConfig {
public:
  RouteTransformationFilterConfig(const RouteTransformationConfigProto &proto_config)
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

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
