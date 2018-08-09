
#pragma once

#include "extensions/filters/http/aws/function_retriever.h"

#include "gmock/gmock.h"

namespace Envoy {
namespace Http {

class MockFunctionRetriever : public FunctionRetriever {
public:
  MockFunctionRetriever();
  ~MockFunctionRetriever();

  MOCK_CONST_METHOD1(getFunction, absl::optional<Function>(
                                      const MetadataAccessor &metadataccessor));

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
