#pragma once

#include <memory>

#include "envoy/common/optional.h"
#include "envoy/common/pure.h"
#include "envoy/http/metadata_accessor.h"
#include "envoy/router/router.h"
#include "envoy/upstream/upstream.h"

#include "common/http/filter/function.h"

namespace Envoy {
namespace Http {

class FunctionRetriever {
public:
  virtual ~FunctionRetriever() {}
  virtual Optional<Function>
  getFunction(const MetadataAccessor &metadataccessor) const PURE;
};

typedef std::shared_ptr<FunctionRetriever> FunctionRetrieverSharedPtr;

} // namespace Http
} // namespace Envoy
