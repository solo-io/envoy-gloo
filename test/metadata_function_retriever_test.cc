#include <iostream>

#include "common/config/solo_well_known_names.h"
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

  MetadataFunctionRetriever functionRetriever(
      Config::SoloMetadataFilters::get().LAMBDA,
      Config::MetadataLambdaKeys::get().FUNC_NAME,
      Config::MetadataLambdaKeys::get().HOSTNAME,
      Config::MetadataLambdaKeys::get().REGION);

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
      Config::MetadataLambdaKeys::get().FUNC_NAME,
      configuredFunction.func_name_, Config::MetadataLambdaKeys::get().HOSTNAME,
      configuredFunction.hostname_, Config::MetadataLambdaKeys::get().REGION,
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
      Config::MetadataLambdaKeys::get().FUNC_NAME,
      configuredFunction.func_name_, Config::MetadataLambdaKeys::get().REGION,
      configuredFunction.region_);

  auto actualFunction = getFunction(json);

  EXPECT_FALSE(actualFunction.valid());
}

TEST(MetadataFunctionRetrieverTest, MisconfiguredFunctionNonStringField) {
  Function configuredFunction{"FunctionName", "lambda.us-east-1.amazonaws.com",
                              "us-east-1"};

  std::string json = fmt::format(
      R"EOF(
    {{
      "{}" : "{}",
      "{}" : 17,
      "{}" : "{}",
    }}
    )EOF",
      Config::MetadataLambdaKeys::get().FUNC_NAME,
      configuredFunction.func_name_, Config::MetadataLambdaKeys::get().HOSTNAME,
      Config::MetadataLambdaKeys::get().REGION, configuredFunction.region_);

  auto actualFunction = getFunction(json);

  EXPECT_FALSE(actualFunction.valid());
}

TEST(MetadataFunctionRetrieverTest, MisconfiguredFunctionEmptyField) {
  std::string empty;
  Function configuredFunction{"FunctionName", "lambda.us-east-1.amazonaws.com",
                              "us-east-1"};

  for (auto func_name : {empty, configuredFunction.func_name_}) {
    for (auto hostname : {empty, configuredFunction.hostname_}) {
      for (auto region : {empty, configuredFunction.region_}) {
        std::string json = fmt::format(
            R"EOF(
          {{
            "{}" : "{}",
            "{}" : "{}",
            "{}" : "{}",
          }}
          )EOF",
            Config::MetadataLambdaKeys::get().FUNC_NAME, func_name,
            Config::MetadataLambdaKeys::get().HOSTNAME, hostname,
            Config::MetadataLambdaKeys::get().REGION, region);

        auto actualFunction = getFunction(json);

        if (func_name.empty() || hostname.empty() || region.empty()) {
          EXPECT_FALSE(actualFunction.valid());
        }
      }
    }
  }
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
      Config::MetadataLambdaKeys::get().HOSTNAME, configuredFunction.hostname_,
      Config::MetadataLambdaKeys::get().REGION, configuredFunction.region_);

  auto actualFunction = getFunction(json);

  EXPECT_FALSE(actualFunction.valid());
}

} // namespace Envoy
