#pragma once

#include <memory>

#include "common/http/filter/function.h"
#include "common/http/filter/function_retriever.h"

namespace Envoy {
namespace Http {

class MetadataFunctionRetriever : public FunctionRetriever {
public:
  MetadataFunctionRetriever();
  // Optional<Function> getFunction(const RouteEntry &routeEntry,
  //                                const ClusterInfo &info) override;
  // Optional<Function> getFunction(const std::string &cluster_name);
  Optional<Function> getFunction(const FunctionalFilterBase& filter) override;
};

} // namespace Http
} // namespace Envoy
