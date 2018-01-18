#include "map_function_retriever.h"

namespace Envoy {
namespace Http {

MapFunctionRetriever::MapFunctionRetriever(ClusterFunctionMap &&functions)
    : functions_(functions) {}

const Function *
MapFunctionRetriever::getFunction(const std::string &cluster_name) {
  auto currentFunction = functions_.find(cluster_name);
  if (currentFunction == functions_.end()) {
    return nullptr;
  }

  return &(currentFunction->second);
}

} // namespace Http
} // namespace Envoy
