#include <iostream>

#include "common/config/lambda_well_known_names.h"
#include "common/http/filter/metadata_function_retriever.h"
#include "common/protobuf/utility.h"

#include "test/test_common/utility.h"

#include "fmt/format.h"

namespace Envoy {
namespace Http {

bool operator==(const Function &lhs, const Function &rhs) {
  bool ret = std::tie(*lhs.name_, *lhs.region_, *lhs.host_, *lhs.access_key_,
                      *lhs.secret_key_, lhs.async_) ==
             std::tie(*rhs.name_, *rhs.region_, *rhs.host_, *rhs.access_key_,
                      *rhs.secret_key_, rhs.async_);

  ret = ret && (!rhs.qualifier_.has_value() == !rhs.qualifier_.has_value());

  if (ret && rhs.qualifier_.has_value()) {
    ret = ret && (*rhs.qualifier_.value() == *rhs.qualifier_.value());
  }

  return ret;
}

namespace {

const std::string empty_json = R"EOF(
    {
    }
    )EOF";

ProtobufWkt::Struct getMetadata(const std::string &json) {
  ProtobufWkt::Struct metadata;
  MessageUtil::loadFromJson(json, metadata);

  return metadata;
}

class TesterMetadataAccessor : public MetadataAccessor {
public:
  virtual absl::optional<const std::string *> getFunctionName() const {
    if (!function_name_.empty()) {
      return &function_name_;
    }
    return {};
  }

  virtual absl::optional<const ProtobufWkt::Struct *> getFunctionSpec() const {
    if (function_spec_ != nullptr) {
      return function_spec_;
    }
    return {};
  }

  virtual absl::optional<const ProtobufWkt::Struct *>
  getClusterMetadata() const {
    if (cluster_metadata_ != nullptr) {
      return cluster_metadata_;
    }
    return {};
  }

  virtual absl::optional<const ProtobufWkt::Struct *> getRouteMetadata() const {
    if (route_metadata_ != nullptr) {
      return route_metadata_;
    }
    return {};
  }

  std::string function_name_;
  const ProtobufWkt::Struct *function_spec_;
  const ProtobufWkt::Struct *cluster_metadata_;
  const ProtobufWkt::Struct *route_metadata_;
};

absl::optional<Function>
getFunctionFromMetadata(const ProtobufWkt::Struct &func_metadata,
                        const ProtobufWkt::Struct &cluster_metadata,
                        const ProtobufWkt::Struct *route_metadata = nullptr) {
  TesterMetadataAccessor testaccessor;
  testaccessor.function_spec_ = &func_metadata;
  testaccessor.cluster_metadata_ = &cluster_metadata;
  testaccessor.route_metadata_ = route_metadata;

  MetadataFunctionRetriever functionRetriever;

  return functionRetriever.getFunction(testaccessor);
}

} // namespace

class MetadataFunctionRetrieverTest : public testing::Test {
public:
  ProtobufWkt::Struct func_metadata_;
  ProtobufWkt::Struct cluster_metadata_;
  ProtobufWkt::Struct route_metadata_;

  std::unique_ptr<Function> configured_function_;

  void SetUp() override { buildfunc(); }
  void buildfunc() {
    configured_function_.reset(new Function(&name_, &qualifier_, async_, &host_,
                                            &region_, &access_key_,
                                            &secret_key_));
  }

  absl::optional<Function> getFunctionFromJson(const std::string &func_json,
                                               const std::string &cluster_json,
                                               std::string route_json = "") {
    func_metadata_ = getMetadata(func_json);
    cluster_metadata_ = getMetadata(cluster_json);

    ProtobufWkt::Struct *route_metadata_ptr = nullptr;
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
                          Config::LambdaMetadataKeys::get().HOSTNAME, host_);
    }
    if (!region_.empty()) {
      json += fmt::format("\"{}\" : \"{}\",",
                          Config::LambdaMetadataKeys::get().REGION, region_);
    }
    if (!access_key_.empty()) {
      json += fmt::format("\"{}\" : \"{}\",",
                          Config::LambdaMetadataKeys::get().ACCESS_KEY,
                          access_key_);
    }
    if (!secret_key_.empty()) {
      json += fmt::format("\"{}\" : \"{}\"",
                          Config::LambdaMetadataKeys::get().SECRET_KEY,
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
        Config::LambdaMetadataKeys::get().FUNC_NAME, name_,
        Config::LambdaMetadataKeys::get().FUNC_QUALIFIER, qualifier_);
  }

  std::string getRouteJson() {
    return fmt::format(
        R"EOF(
    {{
      "{}" : {}
    }}
    )EOF",
        Config::LambdaMetadataKeys::get().FUNC_ASYNC,
        async_ ? "true" : "false");
  }

  absl::optional<Function> getFunction() {
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

  EXPECT_FALSE(function.has_value());
}

TEST_F(MetadataFunctionRetrieverTest, EmptyRouteJson) {
  const std::string &func_json = empty_json;
  std::string cluster_json = getClusterJson();

  auto function = getFunctionFromJson(func_json, cluster_json);

  EXPECT_FALSE(function.has_value());
}

TEST_F(MetadataFunctionRetrieverTest, EmptyClusterJson) {
  std::string func_json = getFuncJson();
  std::string cluster_json = empty_json;

  auto function = getFunctionFromJson(func_json, cluster_json);

  EXPECT_FALSE(function.has_value());
}

TEST_F(MetadataFunctionRetrieverTest, ConfiguredFunction) {

  std::string func_json = getFuncJson();
  std::string cluster_json = getClusterJson();
  std::string route_json = getRouteJson();

  auto actualFunction =
      getFunctionFromJson(func_json, cluster_json, route_json);

  EXPECT_TRUE(actualFunction.has_value());
  EXPECT_EQ(actualFunction.value(), *configured_function_);
}
TEST_F(MetadataFunctionRetrieverTest, ConfiguredFunctionNoRoute) {
  // no route makes async false.
  async_ = false;
  buildfunc();

  std::string func_json = getFuncJson();
  std::string cluster_json = getClusterJson();

  auto actualFunction = getFunctionFromJson(func_json, cluster_json);

  EXPECT_TRUE(actualFunction.has_value());
  EXPECT_EQ(actualFunction.value(), *configured_function_);
}

TEST_F(MetadataFunctionRetrieverTest, MisconfiguredFunctionOppositeJsons) {
  // The cluster metadata JSON is used as the func metadata, and vice versa.
  std::string func_json = getClusterJson();
  std::string cluster_json = getFuncJson();

  auto actualFunction = getFunctionFromJson(func_json, cluster_json);

  EXPECT_FALSE(actualFunction.has_value());
}

TEST_F(MetadataFunctionRetrieverTest, MisconfiguredFunctionMissingField) {

  // delete the hostname line
  host_ = "";

  std::string func_json = getFuncJson();

  // The hostname is missing.
  std::string cluster_json = getClusterJson();

  auto actualFunction = getFunctionFromJson(func_json, cluster_json);

  EXPECT_FALSE(actualFunction.has_value());
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

  EXPECT_FALSE(actualFunction.has_value());
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
          EXPECT_FALSE(actualFunction.has_value());
        } else {
          EXPECT_TRUE(actualFunction.has_value());
        }
      }
    }
  }
}

TEST_F(MetadataFunctionRetrieverTest, MisconfiguredFunctionIncorrectFieldName) {

  // The function name key is incorrect.

  std::string funcnamekey = "NunctionFame";

  std::string func_json = getFuncJson();
  std::string key = Config::LambdaMetadataKeys::get().FUNC_NAME;
  // The hostname is an integer.
  if (func_json.find(key) == std::string::npos) {
    // broken test
    FAIL();
  }
  func_json.replace(func_json.find(key), key.size(), funcnamekey);

  std::string cluster_json = getClusterJson();

  auto actualFunction = getFunctionFromJson(func_json, cluster_json);

  EXPECT_FALSE(actualFunction.has_value());
}

} // namespace Http
} // namespace Envoy
