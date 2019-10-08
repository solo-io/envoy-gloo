#pragma once

#include <string>

#include "envoy/buffer/buffer.h"
#include "envoy/http/header_map.h"
#include "envoy/http/filter.h"
#include "envoy/router/router.h"
#include "common/http/header_utility.h"

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

class TransormConfig {
public:
  virtual ~TransormConfig() {}
  
  virtual TransformerConstSharedPtr getRequestTranformation() const PURE;
  virtual bool shouldClearCache() const PURE;
  virtual TransformerConstSharedPtr getResponseTranformation() const PURE;

};

class FilterConfig : public TransormConfig {
public:
  FilterConfig(const Protobuf::RepeatedPtrField<envoy::api::v2::route::HeaderMatcher>& header_matchers, 
    const std::string& prefix, Stats::Scope& scope) : stats_(generateStats(prefix, scope)), 
    header_matchers_(Envoy::Http::HeaderUtility::buildHeaderDataVector(header_matchers)) {};

  static TransformationFilterStats generateStats(const std::string& prefix, Stats::Scope& scope) {
    const std::string final_prefix = prefix + "transformation.";
    return {ALL_TRANSFORMATION_FILTER_STATS(POOL_COUNTER_PREFIX(scope, final_prefix))};
  }

  
  /**
   * See if any of the matchers specified, matched the given headers.
   * @param request_headers supplies the headers from the request.
   * @param config_headers supplies the list of configured header conditions on which to match.
   * @return bool true if one of the headers (and values) in the config_headers are found in the
   *         request_headers.
  */
  static bool matchHeaders(const Http::HeaderMap& request_headers,
      const std::vector<Envoy::Http::HeaderUtility::HeaderDataPtr>& config_headers) {
    // no headers to match is considered a match
    if (config_headers.empty()) {
      return true;
    } else {  
      for (const Envoy::Http::HeaderUtility::HeaderDataPtr& cfg_header_data : config_headers) {
        if (Envoy::Http::HeaderUtility::matchHeaders(request_headers, *cfg_header_data)) {
          return true;
        }
      }
    }

    return false;
  }
  
  bool matchHeaders(const Http::HeaderMap& request_headers) {
    return matchHeaders(request_headers, header_matchers_);
  }

  TransformationFilterStats& stats() { return stats_; }

  const std::vector<Envoy::Http::HeaderUtility::HeaderDataPtr>& header_matchers() { return header_matchers_; }

  virtual std::string name() const PURE;   

private: 
  TransformationFilterStats stats_;

  const std::vector<Envoy::Http::HeaderUtility::HeaderDataPtr> header_matchers_;
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
