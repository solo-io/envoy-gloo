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
class CacheFilterNameValues {
public:
  // Cache filter
  const std::string CACHE = "io.solo.cache";
};

typedef ConstSingleton<CacheFilterNameValues> CacheFilterNames;

/**
 * Well-known metadata filter namespaces.
 */
class CacheMetadataFilterValues {
public:
  // Filter namespace for cache Filter.
  const std::string CACHE = "io.solo.cache";
};

typedef ConstSingleton<CacheMetadataFilterValues> CacheMetadataFilters;

} // namespace Config
} // namespace Envoy
