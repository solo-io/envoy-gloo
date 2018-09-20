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
  const std::string AWS_LAMBDA = "io.solo.aws_lambda";
};

typedef ConstSingleton<SoloHttpFilterNameValues> SoloHttpFilterNames;

} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
