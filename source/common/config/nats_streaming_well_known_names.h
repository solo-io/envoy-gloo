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
class NatsStreamingHttpFilterNameValues {
public:
  // NATS Streaming filter
  const std::string NATS_STREAMING = "io.solo.nats_streaming";
};

typedef ConstSingleton<NatsStreamingHttpFilterNameValues>
    NatsStreamingHttpFilterNames;

/**
 * Well-known metadata filter namespaces.
 */
class NatsStreamingMetadataFilterValues {
public:
  // Filter namespace for NATS Streaming Filter.
  const std::string NATS_STREAMING = "io.solo.nats_streaming";
};

typedef ConstSingleton<NatsStreamingMetadataFilterValues>
    NatsStreamingMetadataFilters;

} // namespace Config
} // namespace Envoy
