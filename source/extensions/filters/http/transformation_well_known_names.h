#pragma once

#include <string>

#include "common/singleton/const_singleton.h"

namespace Envoy {
namespace Config {

// TODO(yuval-k): TODO: Merge with
// envoy/source/extensions/filters/http/well_known_names.h.

/**
 * Well-known http filter names.
 */
class TransformationFilterNameValues {
public:
  // Transformation filter
  const std::string TRANSFORMATION = "io.solo.transformation";
};

typedef ConstSingleton<TransformationFilterNameValues>
    TransformationFilterNames;

/**
 * Well-known metadata filter namespaces.
 */
class TransformationMetadataFilterValues {
public:
  // Filter namespace for Transformation Filter.
  const std::string TRANSFORMATION = "io.solo.transformation";
};

typedef ConstSingleton<TransformationMetadataFilterValues>
    TransformationMetadataFilters;

/**
 * Keys for MetadataFilterConstants::TRANSFORMATION metadata.
 */
class MetadataTransformationKeyValues {
public:
  const std::string REQUEST_TRANSFORMATION = "request-transformation";
  const std::string RESPONSE_TRANSFORMATION = "response-transformation";
};

typedef ConstSingleton<MetadataTransformationKeyValues>
    MetadataTransformationKeys;

} // namespace Config
} // namespace Envoy
