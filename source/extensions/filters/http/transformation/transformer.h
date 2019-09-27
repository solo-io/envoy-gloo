#pragma once

#include <string>

#include "envoy/buffer/buffer.h"
#include "envoy/http/header_map.h"
#include "envoy/router/router.h"

#include "envoy/stats/scope.h"
#include "envoy/stats/stats_macros.h"

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
                         Buffer::Instance &body) const PURE;
};

typedef std::shared_ptr<Transformer> TransformerSharedPtr;
typedef std::shared_ptr<const Transformer> TransformerConstSharedPtr;

class TransormConfig {
public:
  virtual ~TransormConfig() {}
  
  virtual TransformerConstSharedPtr getRequestTranformation() const PURE;
  virtual bool shouldClearCache() const PURE;
  virtual TransformerConstSharedPtr getResponseTranformation() const PURE;

};

class FilterConfig : public TransormConfig {
public:
  FilterConfig(const std::string& prefix, Stats::Scope& scope) : stats_(generateStats(prefix, scope)) {};

  static TransformationFilterStats generateStats(const std::string& prefix, Stats::Scope& scope) {
    const std::string final_prefix = prefix + "transformation.";
    return {ALL_TRANSFORMATION_FILTER_STATS(POOL_COUNTER_PREFIX(scope, final_prefix))};
  }

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
