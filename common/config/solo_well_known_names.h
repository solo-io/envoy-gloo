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
  // Functional Router filter
  const std::string FUNCTIONAL_ROUTER = "io.solo.function_router";
};

typedef ConstSingleton<SoloHttpFilterNameValues> SoloHttpFilterNames;

/**
 * Well-known metadata filter namespaces.
 */
class SoloMetadataFilterValues {
public:
  // Filter namespace for Functional Router Filter.
  const std::string FUNCTIONAL_ROUTER = "io.solo.function_router";
};

typedef ConstSingleton<SoloMetadataFilterValues> SoloMetadataFilters;

/**
 * Keys for MetadataFilterConstants::NATS_STREAMING metadata.
 */
class MetadataFunctionalRouterKeyValues {
public:
  // Key in the Functional Router Filter namespace for function value.
  const std::string FUNCTION = "function";
  const std::string FUNCTIONS = "functions";
};

typedef ConstSingleton<MetadataFunctionalRouterKeyValues>
    MetadataFunctionalRouterKeys;

} // namespace Config
} // namespace Envoy
