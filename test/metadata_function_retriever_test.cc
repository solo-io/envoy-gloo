#include <iostream>

#include "common/protobuf/utility.h"

#include "test/test_common/utility.h"

#include "fmt/format.h"
#include "metadata_function_retriever.h"

namespace Envoy {

using Http::Function;
using Http::MetadataFunctionRetriever;

namespace {

Optional<Function> getFunction(const std::string json) {
  Protobuf::Struct lambda_metadata;
  MessageUtil::loadFromJson(json, lambda_metadata);

  MetadataFunctionRetriever functionRetriever;
  return functionRetriever.getFunction(lambda_metadata.fields());
}

} // namespace

TEST(MetadataFunctionRetrieverTest, EmptyFunctionMap) {
  std::string json = R"EOF(
    {
    }
    )EOF";

  auto function = getFunction(json);

  EXPECT_FALSE(function.valid());
}

TEST(MetadataFunctionRetrieverTest, ConfiguredFunction) {
  Function configuredFunction{"FunctionName", "lambda.us-east-1.amazonaws.com",
                              "us-east-1"};

  std::string json = fmt::format(
      R"EOF(
    {{
      "{}" : "{}",
      "{}" : "{}",
      "{}" : "{}",
    }}
    )EOF",
      MetadataFunctionRetriever::FUNCTION_FUNC_NAME,
      configuredFunction.func_name_,
      MetadataFunctionRetriever::FUNCTION_HOSTNAME,
      configuredFunction.hostname_, MetadataFunctionRetriever::FUNCTION_REGION,
      configuredFunction.region_);

  auto actualFunction = getFunction(json);

  EXPECT_TRUE(actualFunction.valid());
  EXPECT_EQ(actualFunction.value(), configuredFunction);
}

TEST(MetadataFunctionRetrieverTest, MisconfiguredFunctionMissingField) {
  Function configuredFunction{"FunctionName", "lambda.us-east-1.amazonaws.com",
                              "us-east-1"};

  std::string json = fmt::format(
      R"EOF(
    {{
      "{}" : "{}",
      "{}" : "{}",
    }}
    )EOF",
      MetadataFunctionRetriever::FUNCTION_FUNC_NAME,
      configuredFunction.func_name_, MetadataFunctionRetriever::FUNCTION_REGION,
      configuredFunction.region_);

  auto actualFunction = getFunction(json);

  EXPECT_FALSE(actualFunction.valid());
}

TEST(MetadataFunctionRetrieverTest, MisconfiguredFunctionIncorrectFieldName) {
  Function configuredFunction{"FunctionName", "lambda.us-east-1.amazonaws.com",
                              "us-east-1"};

  std::string json = fmt::format(
      R"EOF(
    {{
      "{}" : "{}",
      "{}" : "{}",
      "{}" : "{}",
    }}
    )EOF",
      "NunctionFame", configuredFunction.func_name_,
      MetadataFunctionRetriever::FUNCTION_HOSTNAME,
      configuredFunction.hostname_, MetadataFunctionRetriever::FUNCTION_REGION,
      configuredFunction.region_);

  auto actualFunction = getFunction(json);

  EXPECT_FALSE(actualFunction.valid());
}

} // namespace Envoy
