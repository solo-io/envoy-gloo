#pragma once

#include <memory>

#include "common/http/filter/function.h"
#include "common/http/filter/function_retriever.h"

namespace Envoy {
namespace Http {

class MetadataFunctionRetriever : public FunctionRetriever {
public:
  MetadataFunctionRetriever();

  Optional<Function>
  getFunction(const MetadataAccessor &metadataccessor) const override;

private:
  static Optional<const std::string *>
  nonEmptyStringValue(const Protobuf::Struct &spec, const std::string &key);

  static bool boolValue(const Protobuf::Struct &spec, const std::string &key);

  static Optional<const Protobuf::Value *> value(const Protobuf::Struct &spec,
                                                 const std::string &key);
};

} // namespace Http
} // namespace Envoy
