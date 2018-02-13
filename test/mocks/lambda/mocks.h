
#pragma once

#include "common/http/filter/function_retriever.h"

#include "gmock/gmock.h"

namespace Envoy {
namespace Http {

class MockFunctionRetriever : public FunctionRetriever {
public:
  MockFunctionRetriever();
  ~MockFunctionRetriever();

  MOCK_CONST_METHOD1(getFunction,
                     Optional<Function>(const FunctionalFilterBase &filter));

  MOCK_CONST_METHOD3(getFunctionFromSpec,
                     Optional<Function>(const Protobuf::Struct &function_spec,
                                        const Protobuf::Struct &upstream_spec,
                                        const ProtobufWkt::Struct *route_spec));

  std::string name_{"name"};
  std::string qualifier_{"qualifier"};
  bool async_{false};
  std::string host_{"host"};
  std::string region_{"region"};
  std::string access_key_{"access_key"};
  std::string secret_key_{"secret_key"};
};

} // namespace Http
} // namespace Envoy
