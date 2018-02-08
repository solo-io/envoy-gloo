#pragma once

#include <memory>

#include "envoy/common/optional.h"
#include "envoy/common/pure.h"
#include "envoy/router/router.h"
#include "envoy/upstream/upstream.h"

#include "function.h"

namespace Envoy {
namespace Http {

using Router::RouteEntry;
using Upstream::ClusterInfo;

class FunctionRetriever {
public:
  virtual ~FunctionRetriever() {}
  virtual Optional<Function> getFunction(const RouteEntry &routeEntry,
                                         const ClusterInfo &info) PURE;
};

typedef std::shared_ptr<FunctionRetriever> FunctionRetrieverSharedPtr;

} // namespace Http
} // namespace Envoy
