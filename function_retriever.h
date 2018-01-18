#pragma once

#include <memory>

#include "function.h"

namespace Envoy {
namespace Http {

class FunctionRetriever {
public:
  FunctionRetriever(ClusterFunctionMap &&functions);
  const Function *getFunction(const std::string &cluster_name);

private:
  ClusterFunctionMap functions_;
};

typedef std::shared_ptr<FunctionRetriever> FunctionRetrieverSharedPtr;

} // namespace Http
} // namespace Envoy
