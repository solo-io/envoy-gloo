#include "envoy/common/optional.h"
#include "common/protobuf/protobuf.h"

namespace Envoy {
namespace Http {

/**
 * This interface is helper to get metadata structs from various 
 * objects in the current filter context.
 */
class MetadataAccessor {
public:
  // Get the function to route to in the current cluster.
  virtual Optional<const ProtobufWkt::Struct *> getFunctionSpec() const PURE;
  // Get the cluster metadata for the current filter
  virtual Optional<const ProtobufWkt::Struct *> getClusterMetadata() const PURE;
  // Get the route metadata for the current filter
  virtual Optional<const ProtobufWkt::Struct *> getRouteMetadata() const PURE;

  virtual ~MetadataAccessor(){}
};

}
}