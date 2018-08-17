#pragma once

#include <string>

#include "common/singleton/const_singleton.h"

namespace Envoy {
namespace Config {

// TODO(talnordan): TODO: Merge with
// envoy/source/extensions/filters/http/well_known_names.h.

/**
 * Well-known http filter names.
 */
class AWSLambdaHttpFilterNameValues {
public:
  // Lambda filter
  const std::string AWS_LAMBDA = "io.solo.aws_lambda";
};

typedef ConstSingleton<AWSLambdaHttpFilterNameValues> AWSLambdaHttpFilterNames;

} // namespace Config
} // namespace Envoy
