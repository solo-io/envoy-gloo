#pragma once

#include "function.h"

namespace Envoy {
namespace Http {

class FunctionRetriever {

  // Disallow construction.
  FunctionRetriever() = delete;
  FunctionRetriever(const FunctionRetriever &) = delete;

public:
  static const Function *getFunction(const ClusterFunctionMap &functions,
                                     const std::string &cluster_name);
};

} // namespace Http
} // namespace Envoy
