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
class SoloCommonFilterNameValues {
public:
  // Functional Router filter
  const std::string FUNCTIONAL_ROUTER = "io.solo.function_router";
  // Functional Router filter
  const std::string ROUTE_FAULT = "io.solo.route_fault";
};

typedef ConstSingleton<SoloCommonFilterNameValues> SoloCommonFilterNames;

/**
 * Well-known metadata filter namespaces.
 */
class SoloCommonMetadataFilterValues {
public:
  // Filter namespace for Functional Router Filter.
  const std::string FUNCTIONAL_ROUTER = "io.solo.function_router";
};

typedef ConstSingleton<SoloCommonMetadataFilterValues>
    SoloCommonMetadataFilters;

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

/**
 * Keys for MetadataFilterConstants::ROUTE_FAULT metadata.
 */
class MetadataRouteFaultKeyValues {
public:
  // Key in the Route Fault Filter namespace for the fault name.
  const std::string FAULT_NAME = "fault_name";
};

typedef ConstSingleton<MetadataRouteFaultKeyValues> MetadataRouteFaultKeys;

} // namespace Config
} // namespace Envoy
