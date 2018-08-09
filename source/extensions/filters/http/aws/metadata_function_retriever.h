#pragma once

#include <memory>

#include "extensions/filters/http/aws/function.h"
#include "extensions/filters/http/aws/function_retriever.h"

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
