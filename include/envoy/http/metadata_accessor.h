#pragma once

#include "envoy/common/optional.h"
#include "envoy/common/pure.h"

#include "common/protobuf/protobuf.h"

namespace Envoy {
namespace Http {

class FilterMetadataAccessor {
public:

  // Get the cluster metadata for the current filter
  virtual Optional<const ProtobufWkt::Struct *> getClusterMetadata() const PURE;
  // Get the route metadata for the current filter
  virtual Optional<const ProtobufWkt::Struct *> getRouteMetadata() const PURE;

  virtual ~FilterMetadataAccessor() {}

};
/**
 * This interface is helper to get metadata structs from various
 * objects in the current filter context.
 */
class MetadataAccessor : public FilterMetadataAccessor {
public:
  // Get the name of the function.
  virtual Optional<const std::string *> getFunctionName() const PURE;
  // Get the function to route to in the current cluster.
  virtual Optional<const ProtobufWkt::Struct *> getFunctionSpec() const PURE;

  virtual ~MetadataAccessor() {}
};

/**
 * This interface should be implemented by function filters. the
 * retrieveFunction will be invoked prior to decodeHeaders to allow the filter
 * to get a function.
 * */
class FunctionalFilter {
public:
  // Get a function via the metadata accessor. return true for success, false
  // for failure.
  virtual bool retrieveFunction(const MetadataAccessor &meta_accessor) PURE;

  virtual ~FunctionalFilter() {}
};

} // namespace Http
} // namespace Envoy
