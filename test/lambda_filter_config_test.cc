#include "test/test_common/utility.h"

#include "lambda_filter_config.h"
#include "lambda_filter_config_factory.h"

namespace Envoy {

using Http::LambdaFilterConfig;
using Server::Configuration::LambdaFilterConfigFactory;

namespace {

LambdaFilterConfig
constructLambdaFilterConfigFromJson(const Json::Object &config) {
  auto proto_config = LambdaFilterConfigFactory::translateLambdaFilter(config);
  return LambdaFilterConfig(proto_config);
}

} // namespace

TEST(LambdaFilterConfigTest, NoAccessAndNoSecret) {
  std::string json = R"EOF(
    {
    }
    )EOF";

  Envoy::Json::ObjectSharedPtr json_config =
      Envoy::Json::Factory::loadFromString(json);

  EXPECT_THROW(LambdaFilterConfigFactory::translateLambdaFilter(*json_config),
               Envoy::EnvoyException);
}

TEST(LambdaFilterConfigTest, AccessOnly) {
  std::string json = R"EOF(
    {
      "access_key" : "a"
    }
    )EOF";

  Envoy::Json::ObjectSharedPtr json_config =
      Envoy::Json::Factory::loadFromString(json);

  EXPECT_THROW(LambdaFilterConfigFactory::translateLambdaFilter(*json_config),
               Envoy::EnvoyException);
}

TEST(LambdaFilterConfigTest, SecretOnly) {
  std::string json = R"EOF(
    {
      "secret_key" : "b"
    }
    )EOF";

  Envoy::Json::ObjectSharedPtr json_config =
      Envoy::Json::Factory::loadFromString(json);

  EXPECT_THROW(LambdaFilterConfigFactory::translateLambdaFilter(*json_config),
               Envoy::EnvoyException);
}

TEST(LambdaFilterConfigTest, AccessAndSecret) {
  std::string json = R"EOF(
    {
      "access_key" : "a",
      "secret_key" : "b"
    }
    )EOF";

  Envoy::Json::ObjectSharedPtr json_config =
      Envoy::Json::Factory::loadFromString(json);
  auto config = constructLambdaFilterConfigFromJson(*json_config);

  EXPECT_EQ(config.awsAccess(), "a");
  EXPECT_EQ(config.awsSecret(), "b");
}

} // namespace Envoy
