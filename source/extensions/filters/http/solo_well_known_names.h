#pragma once

#include <string>

#include "common/singleton/const_singleton.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {

// TODO(talnordan): Merge with
// envoy/source/extensions/filters/http/well_known_names.h

/**
 * Well-known Solo.io HTTP filter names.
 * NOTE: New filters should use the well-known name: io.solo.filters.http.name.
 */
class SoloHttpFilterNameValues {
public:
  // AWS Lambda filter
  // TODO(talnordan): Consider "io.solo.filters.http.aws_lambda".
  const std::string AwsLambda = "io.solo.aws_lambda";
  // NATS Streaming filter
  // TODO(talnordan): Consider "io.solo.filters.http.nats_streaming".
  const std::string NATS_STREAMING = "io.solo.nats_streaming";
  // Transformation filter
  // TODO(talnordan): Consider "io.solo.filters.http.transformation".
  const std::string TRANSFORMATION = "io.solo.transformation";
};

typedef ConstSingleton<SoloHttpFilterNameValues> SoloHttpFilterNames;

} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
