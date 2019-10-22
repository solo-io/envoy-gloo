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

using TransformationConfigProto = envoy::api::v2::filter::http::FilterTransformations;
using RouteTransformationConfigProto = envoy::api::v2::filter::http::RouteTransformations;

class TransformationFilterConfig : public FilterConfig {
public:
  TransformationFilterConfig(const TransformationConfigProto &proto_config, const std::string& prefix, Stats::Scope& scope)
      : FilterConfig(prefix, scope) {
    
    for (const auto& rule : proto_config.transformations()) {
      if (!rule.has_match()) {
        continue;
      }
      TransformerConstSharedPtr request_transformation;
      TransformerConstSharedPtr response_transformation;
      if (rule.has_route_transformations()) {
        const auto& route_transformation = rule.route_transformations();
        clear_route_cache_ = route_transformation.clear_route_cache();
        if (route_transformation.has_request_transformation()) {
          request_transformation =
              Transformation::getTransformer(route_transformation.request_transformation());
        }
        if (route_transformation.has_response_transformation()) {
          response_transformation = Transformation::getTransformer(
              route_transformation.response_transformation());
        }
      }
      TransformerPairConstSharedPtr transformer_pair = std::make_unique<TransformerPair>(request_transformation, response_transformation);
      transformer_pairs_.emplace_back(
          Matcher::create(rule.match()),
          transformer_pair
      );
    }
  }

  const std::vector<MatcherTransformerPair>& transformerPairs() const override {
    return transformer_pairs_;
  };

  std::string name() const override {
    return SoloHttpFilterNames::get().Transformation;
  }

  bool shouldClearCache() const override { return clear_route_cache_; }
  
private:
  bool clear_route_cache_{};

  // The list of transformer matchers.
  std::vector<MatcherTransformerPair> transformer_pairs_{};
};


class RouteTransformationFilterConfig : public RouteFilterConfig {
public:
  RouteTransformationFilterConfig(const RouteTransformationConfigProto &proto_config)
      : clear_route_cache_(proto_config.clear_route_cache()) {

    TransformerConstSharedPtr request_transformation;
    TransformerConstSharedPtr response_transformation;

    if (proto_config.has_request_transformation()) {
      request_transformation =
          Transformation::getTransformer(proto_config.request_transformation());
    }
    if (proto_config.has_response_transformation()) {
      response_transformation = Transformation::getTransformer(
          proto_config.response_transformation());
    }
    transformer_pair_  = std::make_shared<TransformerPair>(request_transformation, response_transformation);
  }

  TransformerPairConstSharedPtr findTransformers(const Http::HeaderMap&) const override {
    return transformer_pair_;
  }

  bool shouldClearCache() const override { return clear_route_cache_; }

private:
  TransformerPairConstSharedPtr transformer_pair_;
  bool clear_route_cache_{};
};

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
