#include "test/test_common/utility.h"

#include "function.h"
#include "function_retriever.h"

namespace Envoy {

using Http::ClusterFunctionMap;
using Http::Function;
using Http::FunctionRetriever;

TEST(FunctionRetrieverTest, EmptyFunctionMap) {

  ClusterFunctionMap functions;

  FunctionRetriever functionRetriever(std::move(functions));
  auto function = functionRetriever.getFunction("lambda-func1");
  EXPECT_EQ(function, nullptr);
}

TEST(FunctionRetrieverTest, ExistingCluster) {

  std::string cluster_name{"lambda-func1"};
  Function configuredFunction{"FunctionName", "lambda.us-east-1.amazonaws.com",
                              "us-east-1"};

  ClusterFunctionMap functions = {{cluster_name, configuredFunction}};

  FunctionRetriever functionRetriever(std::move(functions));
  auto actualFunction = functionRetriever.getFunction(cluster_name);

  EXPECT_EQ(*actualFunction, configuredFunction);
}

TEST(FunctionRetrieverTest, MissingCluster) {

  ClusterFunctionMap functions = {
      {"lambda-func1",
       {"FunctionName", "lambda.us-east-1.amazonaws.com", "us-east-1"}}};

  FunctionRetriever functionRetriever(std::move(functions));
  auto function = functionRetriever.getFunction("lambda-func2");

  EXPECT_EQ(function, nullptr);
}

} // namespace Envoy
