#include "function_retriever.h"

namespace Envoy {
namespace Http {

const Function *
FunctionRetriever::getFunction(const ClusterFunctionMap &functions,
                               const std::string &cluster_name) {
  auto currentFunction = functions.find(cluster_name);
  if (currentFunction == functions.end()) {
    return nullptr;
  }

  return &(currentFunction->second);
}

} // namespace Http
} // namespace Envoy
