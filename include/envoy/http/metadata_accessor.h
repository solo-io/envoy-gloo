#pragma once

#include "envoy/common/optional.h"
#include "envoy/common/pure.h"

#include "common/protobuf/protobuf.h"

namespace Envoy {
namespace Http {

/**
 * This interface is helper to get metadata structs from various
 * objects in the current filter context.
 */
class MetadataAccessor {
public:

  virtual Optional<const std::string*> getFunctionName() const PURE;
  // Get the function to route to in the current cluster.
  virtual Optional<const ProtobufWkt::Struct *> getFunctionSpec() const PURE;
  // Get the cluster metadata for the current filter
  virtual Optional<const ProtobufWkt::Struct *> getClusterMetadata() const PURE;
  // Get the route metadata for the current filter
  virtual Optional<const ProtobufWkt::Struct *> getRouteMetadata() const PURE;
// TODO: get function name for things like NATs without predefined topics.

  virtual ~MetadataAccessor() {}
};

} // namespace Http
} // namespace Envoy
