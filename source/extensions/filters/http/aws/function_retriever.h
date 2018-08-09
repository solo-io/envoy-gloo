#pragma once

#include <memory>

#include "envoy/common/pure.h"
#include "envoy/http/metadata_accessor.h"
#include "envoy/router/router.h"
#include "envoy/upstream/upstream.h"

#include "common/http/filter/function.h"

#include "absl/types/optional.h"

namespace Envoy {
namespace Http {

class FunctionRetriever {
public:
  virtual ~FunctionRetriever() {}
  virtual absl::optional<Function>
  getFunction(const MetadataAccessor &metadataccessor) const PURE;
};

typedef std::shared_ptr<FunctionRetriever> FunctionRetrieverSharedPtr;

} // namespace Http
} // namespace Envoy
