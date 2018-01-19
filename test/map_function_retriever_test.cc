#include "test/test_common/utility.h"

#include "function.h"
#include "map_function_retriever.h"

namespace Envoy {

using Http::ClusterFunctionMap;
using Http::Function;
using Http::MapFunctionRetriever;

TEST(MapFunctionRetrieverTest, EmptyFunctionMap) {

  ClusterFunctionMap functions;

  MapFunctionRetriever functionRetriever(std::move(functions));
  auto function = functionRetriever.getFunction("lambda-func1");
  EXPECT_FALSE(function.valid());
}

TEST(MapFunctionRetrieverTest, ExistingCluster) {

  std::string cluster_name{"lambda-func1"};
  Function configuredFunction{"FunctionName", "lambda.us-east-1.amazonaws.com",
                              "us-east-1"};

  ClusterFunctionMap functions = {{cluster_name, configuredFunction}};

  MapFunctionRetriever functionRetriever(std::move(functions));
  auto actualFunction = functionRetriever.getFunction(cluster_name);

  EXPECT_EQ(actualFunction.value(), configuredFunction);
}

TEST(MapFunctionRetrieverTest, MissingCluster) {

  ClusterFunctionMap functions = {
      {"lambda-func1",
       {"FunctionName", "lambda.us-east-1.amazonaws.com", "us-east-1"}}};

  MapFunctionRetriever functionRetriever(std::move(functions));
  auto function = functionRetriever.getFunction("lambda-func2");

  EXPECT_FALSE(function.valid());
}

} // namespace Envoy
