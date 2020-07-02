#pragma once

#include <string>

#include "envoy/buffer/buffer.h"
#include "envoy/http/filter.h"
#include "envoy/http/header_map.h"
#include "envoy/router/router.h"
#include "envoy/stats/scope.h"
#include "envoy/stats/stats_macros.h"

#include "common/http/header_utility.h"
#include "common/matcher/matcher.h"
#include "common/protobuf/protobuf.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

/**
 * All stats for the transformation filter. @see stats_macros.h
 */
#define ALL_TRANSFORMATION_FILTER_STATS(COUNTER)                               \
  COUNTER(request_body_transformations)                                        \
  COUNTER(request_header_transformations)                                      \
  COUNTER(response_header_transformations)                                     \
  COUNTER(response_body_transformations)                                       \
  COUNTER(request_error)                                                       \
  COUNTER(response_error)

/**
 * Wrapper struct for transformation @see stats_macros.h
 */
struct TransformationFilterStats {
  ALL_TRANSFORMATION_FILTER_STATS(GENERATE_COUNTER_STRUCT)
};

class Transformer {
public:
  virtual ~Transformer() {}

  virtual bool passthrough_body() const PURE;

  virtual void transform(Http::RequestOrResponseHeaderMap &map,
                         // request header map. this has the request header map
                         // even when transforming responses.
                         const Http::RequestHeaderMap *request_headers,
                         Buffer::Instance &body,
                         Http::StreamFilterCallbacks &callbacks) const PURE;
};

typedef std::shared_ptr<const Transformer> TransformerConstSharedPtr;

class TransformerPair {
public:
  TransformerPair(TransformerConstSharedPtr request_transformer,
                  TransformerConstSharedPtr response_transformer,
                  bool should_clear_cache);

  TransformerConstSharedPtr getRequestTranformation() const {
    return request_transformation_;
  }

  TransformerConstSharedPtr getResponseTranformation() const {
    return response_transformation_;
  }

  bool shouldClearCache() const { return clear_route_cache_; }

private:
  bool clear_route_cache_{};
  TransformerConstSharedPtr request_transformation_;
  TransformerConstSharedPtr response_transformation_;
};
typedef std::shared_ptr<const TransformerPair> TransformerPairConstSharedPtr;

class TransformConfig {
public:
  virtual ~TransformConfig() {}
  virtual TransformerPairConstSharedPtr
  findTransformers(const Http::RequestHeaderMap &headers) const PURE;
  virtual TransformerConstSharedPtr
  findResponseTransform(const Http::ResponseHeaderMap &headers,
                        StreamInfo::StreamInfo &) const PURE;
};

class StagedTransformConfig {
public:
  virtual ~StagedTransformConfig() {}
  virtual const TransformConfig *
  transformConfigForStage(uint32_t stage) const PURE;
};

class MatcherTransformerPair {
public:
  MatcherTransformerPair(Matcher::MatcherConstPtr matcher,
                         TransformerPairConstSharedPtr transformer_pair)
      : matcher_(matcher), transformer_pair_(transformer_pair) {}

  TransformerPairConstSharedPtr transformer_pair() const {
    return transformer_pair_;
  }

  Matcher::MatcherConstPtr matcher() const { return matcher_; }

private:
  Matcher::MatcherConstPtr matcher_;
  TransformerPairConstSharedPtr transformer_pair_;
};

class FilterConfig : public TransformConfig {
public:
  FilterConfig(const std::string &prefix, Stats::Scope &scope, uint32_t stage)
      : stats_(generateStats(prefix, scope)), stage_(stage){};

  static TransformationFilterStats generateStats(const std::string &prefix,
                                                 Stats::Scope &scope);

  virtual const std::vector<MatcherTransformerPair> &
  transformerPairs() const PURE;

  // Finds the matcher that matched the header
  TransformerPairConstSharedPtr
  findTransformers(const Http::RequestHeaderMap &headers) const override;

  TransformerConstSharedPtr
  findResponseTransform(const Http::ResponseHeaderMap &,
                        StreamInfo::StreamInfo &) const override {
    return nullptr;
  }

  TransformationFilterStats &stats() { return stats_; }

  virtual std::string name() const PURE;

  uint32_t stage() const { return stage_; }

private:
  TransformationFilterStats stats_;
  uint32_t stage_{};
};

class RouteFilterConfig : public Router::RouteSpecificFilterConfig,
                          public StagedTransformConfig {
public:
  RouteFilterConfig();

  const TransformConfig *transformConfigForStage(uint32_t stage) const override;

protected:
  std::vector<std::unique_ptr<const TransformConfig>> stages_;
};

typedef std::shared_ptr<const RouteFilterConfig>
    RouteFilterConfigConstSharedPtr;
typedef std::shared_ptr<FilterConfig> FilterConfigSharedPtr;

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
