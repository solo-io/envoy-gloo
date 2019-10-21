#pragma once

#include <string>

#include "envoy/buffer/buffer.h"
#include "envoy/http/header_map.h"
#include "envoy/http/filter.h"
#include "envoy/router/router.h"
#include "common/http/header_utility.h"
#include "extensions/filters/http/transformation/matcher.h"

#include "envoy/stats/scope.h"
#include "envoy/stats/stats_macros.h"

#include "common/protobuf/protobuf.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {


/**
 * All stats for the transformation filter. @see stats_macros.h
 */
#define ALL_TRANSFORMATION_FILTER_STATS(COUNTER)                                       \
  COUNTER(request_body_transformations)                                                       \
  COUNTER(request_header_transformations)                                                     \
  COUNTER(response_header_transformations)                                                    \
  COUNTER(response_body_transformations)                                                      \
  COUNTER(transformations_skipped)                                                            \
  COUNTER(request_error)                                                                      \
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

  virtual void transform(Http::HeaderMap &map,
                         Buffer::Instance &body,
                         Http::StreamFilterCallbacks &callbacks) const PURE;
};

typedef std::shared_ptr<Transformer> TransformerSharedPtr;
typedef std::shared_ptr<const Transformer> TransformerConstSharedPtr;

class TransformerPair {
public:
  TransformerPair(TransformerConstSharedPtr request_transformer, TransformerConstSharedPtr response_transformer):
    request_transformation_(request_transformer), response_transformation_(response_transformer) {}

  TransformerConstSharedPtr getRequestTranformation() const {
    return request_transformation_;
  }

  TransformerConstSharedPtr getResponseTranformation() const {
    return response_transformation_;
  }
  
private:
  TransformerConstSharedPtr request_transformation_;
  TransformerConstSharedPtr response_transformation_;
};

typedef std::shared_ptr<const TransformerPair> TransformerPairConstSharedPtr;

class MatcherTransformerPair {
public:
  MatcherTransformerPair(MatcherConstPtr matcher, TransformerPairConstSharedPtr transformer_pair)
      : matcher_(matcher), transformer_pair_(transformer_pair) {}

  TransformerPairConstSharedPtr transformer_pair() const {
    return transformer_pair_;
  }

  MatcherConstPtr matcher() const {
    return matcher_;
  }

private:
  MatcherConstPtr matcher_;
  TransformerPairConstSharedPtr transformer_pair_;
};


class TransormConfig {
public:
  virtual ~TransormConfig() {}

  virtual TransformerPairConstSharedPtr findTransformers(const Http::HeaderMap& headers) const PURE;
  
  virtual bool shouldClearCache() const PURE;
};

class FilterConfig : public TransormConfig {
public:
  FilterConfig(const std::string& prefix, Stats::Scope& scope) : stats_(generateStats(prefix, scope)) {};

  static TransformationFilterStats generateStats(const std::string& prefix, Stats::Scope& scope) {
    const std::string final_prefix = prefix + "transformation.";
    return {ALL_TRANSFORMATION_FILTER_STATS(POOL_COUNTER_PREFIX(scope, final_prefix))};
  }

  virtual const std::vector<MatcherTransformerPair>& transformerPairs() const PURE;

    // Finds the matcher that matched the header
  TransformerPairConstSharedPtr findTransformers(const Http::HeaderMap& headers) const override {
    for (const auto& pair : transformerPairs()) {
      if (pair.matcher()->matches(headers)) {
        return pair.transformer_pair();
      }
    }
    return nullptr;
  };

  TransformationFilterStats& stats() { return stats_; }

  virtual std::string name() const PURE;

private: 
  TransformationFilterStats stats_;
};

class RouteFilterConfig : public Router::RouteSpecificFilterConfig, public TransormConfig {};

typedef std::shared_ptr<const RouteFilterConfig>
    RouteFilterConfigConstSharedPtr;
typedef std::shared_ptr<FilterConfig>
    FilterConfigSharedPtr;

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
