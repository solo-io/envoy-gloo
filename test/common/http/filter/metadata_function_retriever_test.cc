#include <iostream>

#include "common/config/solo_well_known_names.h"
#include "common/http/filter/metadata_function_retriever.h"
#include "common/protobuf/utility.h"

#include "test/test_common/utility.h"

#include "fmt/format.h"

namespace Envoy {
namespace Http {

using Http::Function;
using Http::MetadataFunctionRetriever;

bool operator==(const Function &lhs, const Function &rhs) {
  return std::tie(*lhs.name_, *lhs.qualifier_, *lhs.region_, *lhs.host_,
                  *lhs.access_key_, *lhs.secret_key_, lhs.async_) ==
         std::tie(*rhs.name_, *rhs.qualifier_, *rhs.region_, *rhs.host_,
                  *rhs.access_key_, *rhs.secret_key_, rhs.async_);
}

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

Optional<Function>
getFunctionFromMetadata(const Protobuf::Struct &func_metadata,
                        const Protobuf::Struct &cluster_metadata,
                        const Protobuf::Struct *route_metadata = nullptr) {
  MetadataFunctionRetriever functionRetriever;

  return functionRetriever.getFunctionFromSpec(func_metadata, cluster_metadata,
                                               route_metadata);
}

} // namespace

class MetadataFunctionRetrieverTest : public testing::Test {
public:
  Protobuf::Struct func_metadata_;
  Protobuf::Struct cluster_metadata_;
  Protobuf::Struct route_metadata_;

  Function configuredFunction_;

  void SetUp() override { buildfunc(); }
  void buildfunc() {
    configuredFunction_.name_ = &name_;
    configuredFunction_.host_ = &host_;
    configuredFunction_.region_ = &region_;
    configuredFunction_.access_key_ = &access_key_;
    configuredFunction_.secret_key_ = &secret_key_;
    configuredFunction_.qualifier_ = &qualifier_;
    configuredFunction_.async_ = async_;
  }

  Optional<Function> getFunctionFromJson(const std::string &func_json,
                                         const std::string &cluster_json,
                                         std::string route_json = "") {
    func_metadata_ = getMetadata(func_json);
    cluster_metadata_ = getMetadata(cluster_json);

    Protobuf::Struct *route_metadata_ptr = nullptr;
    if (!route_json.empty()) {
      route_metadata_ = getMetadata(route_json);
      route_metadata_ptr = &route_metadata_;
    }

    return getFunctionFromMetadata(func_metadata_, cluster_metadata_,
                                   route_metadata_ptr);
  }

  std::string getClusterJson() {
    std::string json = "{";

    if (!host_.empty()) {
      json += fmt::format("\"{}\" : \"{}\",",
                          Config::MetadataLambdaKeys::get().HOSTNAME, host_);
    }
    if (!region_.empty()) {
      json += fmt::format("\"{}\" : \"{}\",",
                          Config::MetadataLambdaKeys::get().REGION, region_);
    }
    if (!access_key_.empty()) {
      json += fmt::format("\"{}\" : \"{}\",",
                          Config::MetadataLambdaKeys::get().ACCESS_KEY,
                          access_key_);
    }
    if (!secret_key_.empty()) {
      json += fmt::format("\"{}\" : \"{}\"",
                          Config::MetadataLambdaKeys::get().SECRET_KEY,
                          secret_key_);
    }

    json += "}";

    return json;
  }

  std::string getFuncJson() {
    return fmt::format(
        R"EOF(
    {{
      "{}" : "{}",
      "{}" : "{}"
    }}
    )EOF",
        Config::MetadataLambdaKeys::get().FUNC_NAME, name_,
        Config::MetadataLambdaKeys::get().FUNC_QUALIFIER, qualifier_);
  }

  std::string getRouteJson() {
    return fmt::format(
        R"EOF(
    {{
      "{}" : {}
    }}
    )EOF",
        Config::MetadataLambdaKeys::get().FUNC_ASYNC,
        async_ ? "true" : "false");
  }

  Optional<Function> getFunction() {
    std::string func_json = getFuncJson();
    std::string cluster_json = getClusterJson();
    std::string route_json = getRouteJson();

    return getFunctionFromJson(func_json, cluster_json, route_json);
  }

  std::string name_ = "FunctionName";
  std::string host_ = "lambda.us-east-1.amazonaws.com";
  std::string region_ = "us-east-1";
  std::string access_key_ = "as";
  std::string secret_key_ = "secretdonttell";
  std::string qualifier_ = "";
  bool async_ = false;
};

TEST_F(MetadataFunctionRetrieverTest, EmptyJsons) {
  const std::string &func_json = empty_json;
  const std::string &cluster_json = empty_json;

  auto function = getFunctionFromJson(func_json, cluster_json);

  EXPECT_FALSE(function.valid());
}

TEST_F(MetadataFunctionRetrieverTest, EmptyRouteJson) {
  const std::string &func_json = empty_json;
  std::string cluster_json = getClusterJson();

  auto function = getFunctionFromJson(func_json, cluster_json);

  EXPECT_FALSE(function.valid());
}

TEST_F(MetadataFunctionRetrieverTest, EmptyClusterJson) {
  std::string func_json = getFuncJson();
  std::string cluster_json = empty_json;

  auto function = getFunctionFromJson(func_json, cluster_json);

  EXPECT_FALSE(function.valid());
}

TEST_F(MetadataFunctionRetrieverTest, ConfiguredFunction) {

  std::string func_json = getFuncJson();
  std::string cluster_json = getClusterJson();
  std::string route_json = getRouteJson();

  auto actualFunction =
      getFunctionFromJson(func_json, cluster_json, route_json);

  EXPECT_TRUE(actualFunction.valid());
  EXPECT_EQ(actualFunction.value(), configuredFunction_);
}
TEST_F(MetadataFunctionRetrieverTest, ConfiguredFunctionNoRoute) {
  // no route makes async false.
  async_ = false;
  buildfunc();

  std::string func_json = getFuncJson();
  std::string cluster_json = getClusterJson();

  auto actualFunction = getFunctionFromJson(func_json, cluster_json);

  EXPECT_TRUE(actualFunction.valid());
  EXPECT_EQ(actualFunction.value(), configuredFunction_);
}

TEST_F(MetadataFunctionRetrieverTest, MisconfiguredFunctionOppositeJsons) {
  // The cluster metadata JSON is used as the func metadata, and vice versa.
  std::string func_json = getClusterJson();
  std::string cluster_json = getFuncJson();

  auto actualFunction = getFunctionFromJson(func_json, cluster_json);

  EXPECT_FALSE(actualFunction.valid());
}

TEST_F(MetadataFunctionRetrieverTest, MisconfiguredFunctionMissingField) {

  // delete the hostname line
  host_ = "";

  std::string func_json = getFuncJson();

  // The hostname is missing.
  std::string cluster_json = getClusterJson();

  auto actualFunction = getFunctionFromJson(func_json, cluster_json);

  EXPECT_FALSE(actualFunction.valid());
}

TEST_F(MetadataFunctionRetrieverTest, MisconfiguredFunctionNonStringField) {
  host_ = "17";
  std::string hoststring = "\"17\"";

  std::string func_json = getFuncJson();
  std::string cluster_json = getClusterJson();

  // The hostname is an integer.
  if (cluster_json.find(hoststring) == std::string::npos) {
    // broken test
    FAIL();
  }
  cluster_json.replace(cluster_json.find(hoststring), hoststring.size(), "17");

  auto actualFunction = getFunctionFromJson(func_json, cluster_json);

  EXPECT_FALSE(actualFunction.valid());
}

TEST_F(MetadataFunctionRetrieverTest, MisconfiguredFunctionEmptyField) {
  std::string empty;
  std::string orig_func_name = name_;
  std::string orig_host = host_;
  std::string orig_region = region_;
  for (auto func_name : {empty, orig_func_name}) {
    for (auto hostname : {empty, orig_host}) {
      for (auto region : {empty, orig_region}) {
        name_ = func_name;
        host_ = hostname;
        region_ = region;
        auto actualFunction = getFunction();

        if (func_name.empty() || hostname.empty() || region.empty()) {
          EXPECT_FALSE(actualFunction.valid());
        } else {
          EXPECT_TRUE(actualFunction.valid());
        }
      }
    }
  }
}

TEST_F(MetadataFunctionRetrieverTest, MisconfiguredFunctionIncorrectFieldName) {

  // The function name key is incorrect.

  std::string funcnamekey = "NunctionFame";

  std::string func_json = getFuncJson();
  std::string key = Config::MetadataLambdaKeys::get().FUNC_NAME;
  // The hostname is an integer.
  if (func_json.find(key) == std::string::npos) {
    // broken test
    FAIL();
  }
  func_json.replace(func_json.find(key), key.size(), funcnamekey);

  std::string cluster_json = getClusterJson();

  auto actualFunction = getFunctionFromJson(func_json, cluster_json);

  EXPECT_FALSE(actualFunction.valid());
}

} // namespace Http
} // namespace Envoy
