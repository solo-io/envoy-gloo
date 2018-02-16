#pragma once

#include <string>

#include "common/singleton/const_singleton.h"

namespace Envoy {
namespace Config {

// TODO(talnordan): TODO: Merge with
// envoy/source/common/config/well_known_names.h.

/**
 * Well-known http filter names.
 */
class SoloHttpFilterNameValues {
public:
  // Lambda filter
  const std::string LAMBDA = "io.solo.lambda";
};

typedef ConstSingleton<SoloHttpFilterNameValues> SoloHttpFilterNames;

/**
 * Well-known metadata filter namespaces.
 */
class SoloMetadataFilterValues {
public:
  // Filter namespace for Lambda Filter.
  const std::string LAMBDA = "io.solo.lambda";
};

typedef ConstSingleton<SoloMetadataFilterValues> SoloMetadataFilters;

/**
 * Keys for MetadataFilterConstants::LAMBDA metadata.
 */
class MetadataLambdaKeyValues {
public:
  // Key in the Lambda Filter namespace for function name value.
  const std::string FUNC_NAME = "name";
  // Key in the Lambda Filter namespace for function qualifier value.
  const std::string FUNC_QUALIFIER = "qualifier";

  // Key in the Lambda Filter namespace for function name value.
  const std::string FUNC_ASYNC = "async";

  // Key in the Lambda Filter namespace for hostname value.
  const std::string HOSTNAME = "host";
  // Key in the Lambda Filter namespace for region value.
  const std::string REGION = "region";
  // Key in the Lambda Filter namespace for secret key.
  const std::string ACCESS_KEY = "access_key";
  // Key in the Lambda Filter namespace for secret key.
  const std::string SECRET_KEY = "secret_key";
};

typedef ConstSingleton<MetadataLambdaKeyValues> MetadataLambdaKeys;

} // namespace Config
} // namespace Envoy
