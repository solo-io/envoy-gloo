#pragma once

#include <memory>

#include "function.h"

namespace Envoy {
namespace Http {

class MapFunctionRetriever {
public:
  MapFunctionRetriever(ClusterFunctionMap &&functions);
  const Function *getFunction(const std::string &cluster_name);

private:
  ClusterFunctionMap functions_;
};

typedef std::shared_ptr<MapFunctionRetriever> FunctionRetrieverSharedPtr;

} // namespace Http
} // namespace Envoy
