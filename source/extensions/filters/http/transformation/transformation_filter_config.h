#pragma once

#include <string>

#include "envoy/router/router.h"
#include "envoy/config/typed_config.h"

#include "source/extensions/filters/http/solo_well_known_names.h"
#include "source/extensions/filters/http/transformation/filter_config.h"
#include "source/extensions/filters/http/common/factory_base.h"

#include "api/envoy/config/filter/http/transformation/v2/transformation_filter.pb.validate.h"
#include "envoy/server/factory_context.h"

#include "source/common/matcher/data_impl.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

class ResponseMatcher;
using ResponseMatcherConstPtr = std::unique_ptr<const ResponseMatcher>;
class ResponseMatcher {
public:
  virtual ~ResponseMatcher() = default;
  virtual bool matches(const Http::ResponseHeaderMap &headers,
                       const StreamInfo::StreamInfo &stream_info) const PURE;

  /**
   * Factory method to create a shared instance of a matcher based on the rule
   * defined.
   */
  static ResponseMatcherConstPtr
  create(const envoy::api::v2::filter::http::ResponseMatcher &match);
};

using TransformationConfigProto =
    envoy::api::v2::filter::http::FilterTransformations;
using RouteTransformationConfigProto =
    envoy::api::v2::filter::http::RouteTransformations;

class TransformationFilterConfig : public FilterConfig {
public:
  TransformationFilterConfig(const TransformationConfigProto &proto_config,
                             const std::string &prefix, Server::Configuration::FactoryContext &context);

  std::string name() const override {
    return SoloHttpFilterNames::get().Transformation;
  }

  bool logRequestResponseInfo() const {
    return log_request_response_info_;
  }
protected:

  const std::vector<MatcherTransformerPair> &transformerPairs() const override {
    return transformer_pairs_;
  };
  Envoy::Matcher::MatchTreeSharedPtr<Http::HttpMatchingData> matcher() const override {return matcher_;};

private:
  void addTransformationLegacy(const envoy::api::v2::filter::http::TransformationRule& rule, Server::Configuration::FactoryContext &context);

  // The list of transformer matchers.
  std::vector<MatcherTransformerPair> transformer_pairs_{};
  Envoy::Matcher::MatchTreeSharedPtr<Http::HttpMatchingData> matcher_;

  bool log_request_response_info_{};
};

class PerStageRouteTransformationFilterConfig : public TransformConfig {
public:
  PerStageRouteTransformationFilterConfig() = default;
  void addTransformation(
      const envoy::api::v2::filter::http::
          RouteTransformations_RouteTransformation &transformations,
          Server::Configuration::CommonFactoryContext &context);
  void setMatcher(Envoy::Matcher::MatchTreeSharedPtr<Http::HttpMatchingData> matcher);

  TransformerPairConstSharedPtr
  findTransformers(const Http::RequestHeaderMap &headers, StreamInfo::StreamInfo& info) const override;
  TransformerConstSharedPtr
  findResponseTransform(const Http::ResponseHeaderMap &,
                        StreamInfo::StreamInfo &) const override;

private:

  Envoy::Matcher::MatchTreeSharedPtr<Http::HttpMatchingData> matcher_;

  std::vector<MatcherTransformerPair> transformer_pairs_;
  std::vector<std::pair<ResponseMatcherConstPtr, TransformerConstSharedPtr>>
      response_transformations_;
};

class RouteTransformationFilterConfig : public RouteFilterConfig {
public:
  RouteTransformationFilterConfig(RouteTransformationConfigProto proto_config,
    Server::Configuration::ServerFactoryContext &context);
};

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
