#pragma once

#include <memory>

#include "envoy/common/pure.h"

#include "function.h"

namespace Envoy {
namespace Http {

class FunctionRetriever {
public:
  virtual ~FunctionRetriever() {}
  virtual const Function *getFunction(const std::string &cluster_name) PURE;
};

typedef std::shared_ptr<FunctionRetriever> FunctionRetrieverSharedPtr;

} // namespace Http
} // namespace Envoy
