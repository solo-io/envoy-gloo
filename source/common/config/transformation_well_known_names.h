#pragma once

#include <string>

#include "common/singleton/const_singleton.h"

namespace Envoy {
namespace Config {

// TODO(yuval-k): TODO: Merge with
// envoy/source/common/config/well_known_names.h.

/**
 * Well-known http filter names.
 */
class TransformationFilterNameValues {
public:
  // Transformation filter
  const std::string TRANSFORMATION = "io.solo.transformation";
};

typedef ConstSingleton<TransformationFilterNameValues> TransformationFilterNames;

/**
 * Well-known metadata filter namespaces.
 */
class TransformationMetadataFilterValues {
public:
  // Filter namespace for Transformation Filter.
  const std::string TRANSFORMATION = "io.solo.transformation";
};

typedef ConstSingleton<TransformationMetadataFilterValues> TransformationMetadataFilters;

/**
 * Keys for MetadataFilterConstants::TRANSFORMATION metadata.
 */
class MetadataTransformationKeyValues {
public:
};

typedef ConstSingleton<MetadataTransformationKeyValues> MetadataTransformationKeys;

} // namespace Config
} // namespace Envoy
