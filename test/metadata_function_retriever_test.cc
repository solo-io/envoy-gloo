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

const std::string empty_json = R"EOF(
    {
    }
    )EOF";

Protobuf::Struct getMetadata(const std::string &json) {
  Protobuf::Struct metadata;
  MessageUtil::loadFromJson(json, metadata);

  return metadata;
}

std::string getRouteJson(const std::string &function_name) {
  return fmt::format(
      R"EOF(
    {{
      "{}" : "{}",
    }}
    )EOF",
      Config::MetadataLambdaKeys::get().FUNC_NAME, function_name);
}

std::string getClusterJson(const std::string &hostname,
                           const std::string &region) {
  return fmt::format(
      R"EOF(
    {{
      "{}" : "{}",
      "{}" : "{}",
    }}
    )EOF",
      Config::MetadataLambdaKeys::get().HOSTNAME, hostname,
      Config::MetadataLambdaKeys::get().REGION, region);
}

Optional<Function>
getFunctionFromMetadata(const Protobuf::Struct &route_metadata,
                        const Protobuf::Struct &cluster_metadata) {
  MetadataFunctionRetriever functionRetriever(
      Config::SoloMetadataFilters::get().LAMBDA,
      Config::MetadataLambdaKeys::get().FUNC_NAME,
      Config::MetadataLambdaKeys::get().HOSTNAME,
      Config::MetadataLambdaKeys::get().REGION);

  return functionRetriever.getFunction(route_metadata.fields(),
                                       cluster_metadata.fields());
}

Optional<Function> getFunctionFromJson(const std::string &route_json,
                                       const std::string &cluster_json) {
  Protobuf::Struct route_metadata = getMetadata(route_json);
  Protobuf::Struct cluster_metadata = getMetadata(cluster_json);
  return getFunctionFromMetadata(route_metadata, cluster_metadata);
}

Optional<Function> getFunction(const std::string &function_name,
                               const std::string &hostname,
                               const std::string &region) {
  std::string route_json = getRouteJson(function_name);
  std::string cluster_json = getClusterJson(hostname, region);
  return getFunctionFromJson(route_json, cluster_json);
}

} // namespace

TEST(MetadataFunctionRetrieverTest, EmptyJsons) {
  const std::string &route_json = empty_json;
  const std::string &cluster_json = empty_json;

  auto function = getFunctionFromJson(route_json, cluster_json);

  EXPECT_FALSE(function.valid());
}

TEST(MetadataFunctionRetrieverTest, EmptyRouteJson) {
  const std::string &route_json = empty_json;
  std::string cluster_json =
      getClusterJson("lambda.us-east-1.amazonaws.com", "us-east-1");

  auto function = getFunctionFromJson(route_json, cluster_json);

  EXPECT_FALSE(function.valid());
}

TEST(MetadataFunctionRetrieverTest, EmptyClusterJson) {
  std::string route_json = getRouteJson("FunctionName");
  std::string cluster_json = empty_json;

  auto function = getFunctionFromJson(route_json, cluster_json);

  EXPECT_FALSE(function.valid());
}

TEST(MetadataFunctionRetrieverTest, ConfiguredFunction) {
  Function configuredFunction{"FunctionName", "lambda.us-east-1.amazonaws.com",
                              "us-east-1"};

  std::string route_json = getRouteJson(configuredFunction.func_name_);
  std::string cluster_json =
      getClusterJson(configuredFunction.hostname_, configuredFunction.region_);

  auto actualFunction = getFunctionFromJson(route_json, cluster_json);

  EXPECT_TRUE(actualFunction.valid());
  EXPECT_EQ(actualFunction.value(), configuredFunction);
}

TEST(MetadataFunctionRetrieverTest, MisconfiguredFunctionOppositeJsons) {
  Function configuredFunction{"FunctionName", "lambda.us-east-1.amazonaws.com",
                              "us-east-1"};

  // The cluster metadata JSON is used as the route metadata, and vice versa.
  std::string route_json =
      getClusterJson(configuredFunction.hostname_, configuredFunction.region_);
  std::string cluster_json = getRouteJson(configuredFunction.func_name_);

  auto actualFunction = getFunctionFromJson(route_json, cluster_json);

  EXPECT_FALSE(actualFunction.valid());
}

TEST(MetadataFunctionRetrieverTest, MisconfiguredFunctionMissingField) {
  Function configuredFunction{"FunctionName", "lambda.us-east-1.amazonaws.com",
                              "us-east-1"};

  std::string route_json = getRouteJson(configuredFunction.func_name_);

  // The hostname is missing.
  std::string cluster_json = fmt::format(
      R"EOF(
    {{
      "{}" : "{}",
    }}
    )EOF",
      Config::MetadataLambdaKeys::get().REGION, configuredFunction.region_);

  auto actualFunction = getFunctionFromJson(route_json, cluster_json);

  EXPECT_FALSE(actualFunction.valid());
}

TEST(MetadataFunctionRetrieverTest, MisconfiguredFunctionNonStringField) {
  Function configuredFunction{"FunctionName", "lambda.us-east-1.amazonaws.com",
                              "us-east-1"};

  std::string route_json = getRouteJson(configuredFunction.func_name_);

  // The hostname is an integer.
  std::string cluster_json = fmt::format(
      R"EOF(
    {{
      "{}" : 17,
      "{}" : "{}",
    }}
    )EOF",
      Config::MetadataLambdaKeys::get().HOSTNAME,
      Config::MetadataLambdaKeys::get().REGION, configuredFunction.region_);

  auto actualFunction = getFunctionFromJson(route_json, cluster_json);

  EXPECT_FALSE(actualFunction.valid());
}

TEST(MetadataFunctionRetrieverTest, MisconfiguredFunctionEmptyField) {
  std::string empty;
  Function configuredFunction{"FunctionName", "lambda.us-east-1.amazonaws.com",
                              "us-east-1"};

  for (auto func_name : {empty, configuredFunction.func_name_}) {
    for (auto hostname : {empty, configuredFunction.hostname_}) {
      for (auto region : {empty, configuredFunction.region_}) {

        auto actualFunction = getFunction(func_name, hostname, region);

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

  // The function name key is incorrect.
  std::string route_json = fmt::format(
      R"EOF(
    {{
      "{}" : "{}",
    }}
    )EOF",
      "NunctionFame", configuredFunction.func_name_);

  std::string cluster_json =
      getClusterJson(configuredFunction.hostname_, configuredFunction.region_);

  auto actualFunction = getFunctionFromJson(route_json, cluster_json);

  EXPECT_FALSE(actualFunction.valid());
}

} // namespace Envoy
