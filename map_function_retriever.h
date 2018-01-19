#pragma once

#include <memory>

#include "function.h"
#include "function_retriever.h"

namespace Envoy {
namespace Http {

class MapFunctionRetriever : public FunctionRetriever {
public:
  MapFunctionRetriever(ClusterFunctionMap &&functions);
  Optional<Function> getFunction(const std::string &cluster_name) override;

private:
  ClusterFunctionMap functions_;
};

} // namespace Http
} // namespace Envoy
