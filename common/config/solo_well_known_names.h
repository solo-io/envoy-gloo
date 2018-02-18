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
class SoloFunctionalFilterFilterNameValues {
public:
  // Functional Router filter
  const std::string FUNCTIONAL_ROUTER = "io.solo.function_router";
};

typedef ConstSingleton<SoloFunctionalFilterFilterNameValues>
    SoloFunctionalFilterFilterNames;

/**
 * Well-known metadata filter namespaces.
 */
class SoloFunctionalFilterMetadataFilterValues {
public:
  // Filter namespace for Functional Router Filter.
  const std::string FUNCTIONAL_ROUTER = "io.solo.function_router";
};

typedef ConstSingleton<SoloFunctionalFilterMetadataFilterValues>
    SoloFunctionalFilterMetadataFilters;

/**
 * Keys for MetadataFilterConstants::NATS_STREAMING metadata.
 */
class MetadataFunctionalRouterKeyValues {
public:
  // Key in the Functional Router Filter namespace for function value.
  const std::string FUNCTION = "function";
  const std::string WEIGHTED_FUNCTIONS = "weighted_functions";
  const std::string FUNCTIONS = "functions";
  const std::string FUNCTIONS_TOTAL_WEIGHT = "total_weight";
  const std::string WEIGHTED_FUNCTIONS_NAME = "name";
  const std::string WEIGHTED_FUNCTIONS_WEIGHT = "weight";
};

typedef ConstSingleton<MetadataFunctionalRouterKeyValues>
    MetadataFunctionalRouterKeys;

} // namespace Config
} // namespace Envoy
