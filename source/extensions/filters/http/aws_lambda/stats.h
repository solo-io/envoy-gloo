#pragma once

#include "envoy/stats/scope.h"
#include "envoy/stats/stats_macros.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {

class StatsConstantValues {
public:
  const std::string Prefix{"aws_lambda."};
};

using StatsConstants = ConstSingleton<StatsConstantValues>;

/**
 * All stats for the aws filter. @see stats_macros.h
 */
#define ALL_AWS_LAMBDA_FILTER_STATS(COUNTER, GAUGE)                            \
  COUNTER(fetch_failed)                                                        \
  COUNTER(fetch_success)                                                       \
  COUNTER(creds_rotated)                                                       \
  GAUGE(current_state, NeverImport)

/**
 * Wrapper struct for aws filter stats. @see stats_macros.h
 */
struct AwsLambdaFilterStats {
  ALL_AWS_LAMBDA_FILTER_STATS(GENERATE_COUNTER_STRUCT, GENERATE_GAUGE_STRUCT)
};


} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
