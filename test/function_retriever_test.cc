#include "test/test_common/utility.h"

#include "function.h"
#include "function_retriever.h"

namespace Envoy {

using Http::ClusterFunctionMap;
using Http::FunctionRetriever;

TEST(FunctionRetrieverTest, EmptyFunctionMap) {

  ClusterFunctionMap functions;

  auto function = FunctionRetriever::getFunction(functions, "lambda-func1");
  EXPECT_EQ(function, nullptr);
}

TEST(FunctionRetrieverTest, ExistingCluster) {

  ClusterFunctionMap functions = {
      {"lambda-func1",
       {"FunctionName", "lambda.us-east-1.amazonaws.com", "us-east-1"}}};

  auto function = FunctionRetriever::getFunction(functions, "lambda-func1");
  EXPECT_EQ(function, &functions["lambda-func1"]);
}

TEST(FunctionRetrieverTest, MissingCluster) {

  ClusterFunctionMap functions = {
      {"lambda-func1",
       {"FunctionName", "lambda.us-east-1.amazonaws.com", "us-east-1"}}};

  auto function = FunctionRetriever::getFunction(functions, "lambda-func2");
  EXPECT_EQ(function, nullptr);
}

} // namespace Envoy
