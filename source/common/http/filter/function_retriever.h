#pragma once

#include <memory>

#include "envoy/common/optional.h"
#include "envoy/common/pure.h"
#include "envoy/router/router.h"
#include "envoy/upstream/upstream.h"
#include "common/http/functional_stream_decoder_base.h"


#include "common/http/filter/function.h"

namespace Envoy {
namespace Http {

using Router::RouteEntry;
using Upstream::ClusterInfo;

class FunctionRetriever {
public:
  virtual ~FunctionRetriever() {}
  virtual Optional<Function> getFunction(const FunctionalFilterBase& filter) PURE;
};

typedef std::shared_ptr<FunctionRetriever> FunctionRetrieverSharedPtr;

} // namespace Http
} // namespace Envoy
