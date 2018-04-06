#pragma once

#include <memory>

#include "common/http/filter/function.h"
#include "common/http/filter/function_retriever.h"

namespace Envoy {
namespace Http {

class MetadataFunctionRetriever : public FunctionRetriever {
public:
  MetadataFunctionRetriever();

  absl::optional<Function>
  getFunction(const MetadataAccessor &metadataccessor) const override;
};

} // namespace Http
} // namespace Envoy
