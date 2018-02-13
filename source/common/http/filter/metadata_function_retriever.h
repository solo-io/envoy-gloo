#pragma once

#include <memory>

#include "common/http/filter/function.h"
#include "common/http/filter/function_retriever.h"

namespace Envoy {
namespace Http {

class MetadataFunctionRetriever : public FunctionRetriever {
public:
  MetadataFunctionRetriever();
  
  Optional<Function> getFunctionFromSpec(const Protobuf::Struct &function_spec, const Protobuf::Struct &upstream_spec, const ProtobufWkt::Struct *route_spec) const override;
};

} // namespace Http
} // namespace Envoy
