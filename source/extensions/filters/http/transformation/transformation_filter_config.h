#pragma once

#include <string>

#include "envoy/router/router.h"

#include "extensions/filters/http/solo_well_known_names.h"
#include "extensions/filters/http/transformation/transformer.h"

#include "api/envoy/config/filter/http/transformation/v2/transformation_filter.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

class Transformation {
public:
  static TransformerConstSharedPtr getTransformer(
      const envoy::api::v2::filter::http::Transformation &transformation);
};

class ResponseMatcher;
using ResponseMatcherConstPtr = std::unique_ptr<const ResponseMatcher>;
class ResponseMatcher {
  public:
  virtual ~ResponseMatcher() = default;
  virtual bool matches(const Http::ResponseHeaderMap& headers, const StreamInfo::StreamInfo &stream_info) const PURE;

  /**
   * Factory method to create a shared instance of a matcher based on the rule defined.
   */
  static ResponseMatcherConstPtr
    create(const envoy::api::v2::filter::http::ResponseMatcher& match);
};

using TransformationConfigProto =
    envoy::api::v2::filter::http::FilterTransformations;
using RouteTransformationConfigProto =
    envoy::api::v2::filter::http::RouteTransformations;

class TransformationFilterConfig : public FilterConfig {
public:
  TransformationFilterConfig(const TransformationConfigProto &proto_config,
                             const std::string &prefix, Stats::Scope &scope);

  const std::vector<MatcherTransformerPair> &transformerPairs() const override {
    return transformer_pairs_;
  };

  std::string name() const override {
    return SoloHttpFilterNames::get().Transformation;
  }

private:
  // The list of transformer matchers.
  std::vector<MatcherTransformerPair> transformer_pairs_{};
};

class PerStageRouteTransformationFilterConfig : public TransformConfig {
public:
  PerStageRouteTransformationFilterConfig() = default;
  void addTransformation(const envoy::api::v2::filter::http::RouteTransformations_Transformations& transformations);

  TransformerPairConstSharedPtr findTransformers(const Http::RequestHeaderMap& headers) const override;
  TransformerConstSharedPtr findResponseTransform(const Http::ResponseHeaderMap&, StreamInfo::StreamInfo&) const override;
private:
  std::vector<MatcherTransformerPair> transformer_pairs_;
  std::vector<std::pair<ResponseMatcherConstPtr, TransformerConstSharedPtr> > response_transformations_;
};

class RouteTransformationFilterConfig : public RouteFilterConfig {
public:
  RouteTransformationFilterConfig(
      RouteTransformationConfigProto proto_config);

};

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
