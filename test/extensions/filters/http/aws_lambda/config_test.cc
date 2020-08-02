#include "extensions/filters/http/aws_lambda/config.h"

#include "test/extensions/filters/http/aws_lambda/mocks.h"
#include "test/extensions/common/aws/mocks.h"
#include "test/mocks/common.h"
#include "test/mocks/server/mocks.h"
#include "test/mocks/upstream/mocks.h"
#include "test/test_common/utility.h"

#include "fmt/format.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::_;
using testing::AtLeast;
using testing::Invoke;
using testing::Return;
using testing::ReturnPointee;
using testing::ReturnRef;
using testing::SaveArg;
using testing::WithArg;

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {

const std::string service_account_credentials_config = R"(
cluster: test
uri: https://foo.com
timeout: 1s
)";

class ConfigTest : public testing::Test {
public:
  ConfigTest() {}

protected:
  void SetUp() override {}

  NiceMock<Server::Configuration::MockFactoryContext> context_;
  Stats::TestUtil::TestStore stats_;

  NiceMock<MockStsCredentialsProviderFactory> sts_factory_;

  envoy::config::filter::http::aws_lambda::v2::AWSLambdaConfig protoconfig;

  NiceMock<Event::MockTimer> *prepareTimer() {
    NiceMock<Event::MockTimer> *timer =
        new NiceMock<Event::MockTimer>(&context_.dispatcher_);
    protoconfig.mutable_use_default_credentials()->set_value(true);
    EXPECT_CALL(context_.thread_local_, allocateSlot()).Times(1);
    return timer;
  }

   void prepareSTS() {
    envoy::config::filter::http::aws_lambda::v2::AWSLambdaConfig_ServiceAccountCredentials creds_;
    TestUtility::loadFromYaml(service_account_credentials_config, creds_);
    *(protoconfig.mutable_service_account_credentials()) = creds_;
  }
};

TEST_F(ConfigTest, WithUseDefaultCreds) {
  auto timer = prepareTimer();

  envoy::config::filter::http::aws_lambda::v2::AWSLambdaProtocolExtension
      protoextconfig;

  const Envoy::Extensions::Common::Aws::Credentials creds("access_key",
                                                          "secret_key");
                                                          
  const Envoy::Extensions::Common::Aws::Credentials creds2("access_key",
                                                          "secret_key",
                                                          "session_token");

  auto cred_provider = std::make_unique<
      NiceMock<Envoy::Extensions::Common::Aws::MockCredentialsProvider>>();
  EXPECT_CALL(*cred_provider, getCredentials())
      .WillOnce(Return(creds))
    .WillOnce(Return(creds2));

  AWSLambdaConfigImpl config(std::move(cred_provider), context_.cluster_manager_, sts_factory_, context_.dispatcher_,
                             context_.thread_local_, "prefix.", stats_, context_.api_,
                             protoconfig);

  NiceMock<MockStsCallbacks> callbacks_1;

  std::shared_ptr<const AWSLambdaProtocolExtensionConfig> ext_config_1= std::make_shared<const AWSLambdaProtocolExtensionConfig>(protoextconfig);

  EXPECT_CALL(callbacks_1, onSuccess(_)).
    WillOnce(Invoke([&](std::shared_ptr<const Envoy::Extensions::Common::Aws::Credentials> result) -> void {
      EXPECT_EQ(result->accessKeyId().value(), "access_key");
      EXPECT_EQ(result->secretAccessKey().value(),
                "secret_key");
      EXPECT_EQ(result->sessionToken().has_value(), false);
    }));

  EXPECT_EQ(nullptr, config.getCredentials(ext_config_1, &callbacks_1));

  timer->invokeCallback();

  NiceMock<MockStsCallbacks> callbacks_2;
  std::shared_ptr<const AWSLambdaProtocolExtensionConfig> ext_config_2 = std::make_shared<const AWSLambdaProtocolExtensionConfig>(protoextconfig);
  
  EXPECT_CALL(callbacks_2, onSuccess(_)).
    WillOnce(Invoke([&](std::shared_ptr<const Envoy::Extensions::Common::Aws::Credentials> result) -> void {
      EXPECT_EQ(result->accessKeyId().value(), "access_key");
      EXPECT_EQ(result->secretAccessKey().value(),
                "secret_key");
      EXPECT_EQ(result->sessionToken().value(), "session_token");
    }));
  
  EXPECT_EQ(nullptr, config.getCredentials(ext_config_2, &callbacks_2));

  EXPECT_EQ(
      2UL, stats_.counterFromString("prefix.aws_lambda.fetch_success").value());
  EXPECT_EQ(
      2UL, stats_.counterFromString("prefix.aws_lambda.creds_rotated").value());
  EXPECT_EQ(1UL, stats_
                     .gaugeFromString("prefix.aws_lambda.current_state",
                                      Stats::Gauge::ImportMode::NeverImport)
                     .value());
  EXPECT_EQ(0UL,
            stats_.counterFromString("prefix.aws_lambda.fetch_failed").value());
}

TEST_F(ConfigTest, FailingToRotate) {
  auto timer = prepareTimer();

  envoy::config::filter::http::aws_lambda::v2::AWSLambdaProtocolExtension
      protoextconfig;

  const Envoy::Extensions::Common::Aws::Credentials creds("access_key",
                                                          "secret_key");

  auto cred_provider = std::make_unique<
      NiceMock<Envoy::Extensions::Common::Aws::MockCredentialsProvider>>();
  EXPECT_CALL(*cred_provider, getCredentials())
      .WillOnce(Return(creds))
      .WillOnce(Return(Envoy::Extensions::Common::Aws::Credentials()));

  AWSLambdaConfigImpl config(std::move(cred_provider), context_.cluster_manager_, sts_factory_, context_.dispatcher_,
                             context_.thread_local_, "prefix.", stats_, context_.api_,
                             protoconfig);

  std::shared_ptr<const AWSLambdaProtocolExtensionConfig> ext_config_1= std::make_shared<const AWSLambdaProtocolExtensionConfig>(protoextconfig);

  NiceMock<MockStsCallbacks> callbacks_1;

  EXPECT_CALL(callbacks_1, onSuccess(_)).
    Times(2).
    WillRepeatedly(Invoke([&](std::shared_ptr<const Envoy::Extensions::Common::Aws::Credentials> result) -> void {
      EXPECT_EQ(result->accessKeyId().value(), "access_key");
      EXPECT_EQ(result->secretAccessKey().value(),
                "secret_key");
      EXPECT_EQ(result->sessionToken().has_value(), false);
    }));

  EXPECT_EQ(nullptr, config.getCredentials(ext_config_1, &callbacks_1));

  timer->invokeCallback();

  // When we fail to rotate we latch to the last good credentials
  EXPECT_EQ(nullptr, config.getCredentials(ext_config_1, &callbacks_1));

  EXPECT_EQ(
      1UL, stats_.counterFromString("prefix.aws_lambda.fetch_success").value());
  EXPECT_EQ(
      1UL, stats_.counterFromString("prefix.aws_lambda.creds_rotated").value());
  EXPECT_EQ(0UL, stats_
                     .gaugeFromString("prefix.aws_lambda.current_state",
                                      Stats::Gauge::ImportMode::NeverImport)
                     .value());
  EXPECT_EQ(1UL,
            stats_.counterFromString("prefix.aws_lambda.fetch_failed").value());
}

TEST_F(ConfigTest, WithProtocolExtensionCreds) {

  envoy::config::filter::http::aws_lambda::v2::AWSLambdaProtocolExtension
      protoextconfig;
  protoextconfig.set_access_key("access_key");
  protoextconfig.set_secret_key("secret_key");

  auto cred_provider = std::make_unique<
      NiceMock<Envoy::Extensions::Common::Aws::MockCredentialsProvider>>();

  AWSLambdaConfigImpl config(std::move(cred_provider), context_.cluster_manager_,  sts_factory_, context_.dispatcher_,
                             context_.thread_local_, "prefix.", stats_, context_.api_,
                             protoconfig);

  NiceMock<MockStsCallbacks> callbacks_1;

  std::shared_ptr<const AWSLambdaProtocolExtensionConfig> ext_config_1= std::make_shared<const AWSLambdaProtocolExtensionConfig>(protoextconfig);

  EXPECT_CALL(callbacks_1, onSuccess(_)).
    WillOnce(Invoke([&](std::shared_ptr<const Envoy::Extensions::Common::Aws::Credentials> result) -> void {
      EXPECT_EQ(result->accessKeyId().value(), "access_key");
      EXPECT_EQ(result->secretAccessKey().value(),
                "secret_key");
      EXPECT_EQ(result->sessionToken().has_value(), false);
    }));

  EXPECT_EQ(nullptr, config.getCredentials(ext_config_1, &callbacks_1));

  NiceMock<MockStsCallbacks> callbacks_2;
  protoextconfig.set_session_token("session_token");
  std::shared_ptr<const AWSLambdaProtocolExtensionConfig> ext_config_2 = std::make_shared<const AWSLambdaProtocolExtensionConfig>(protoextconfig);
  
  EXPECT_CALL(callbacks_2, onSuccess(_)).
    WillOnce(Invoke([&](std::shared_ptr<const Envoy::Extensions::Common::Aws::Credentials> result) -> void {
      EXPECT_EQ(result->accessKeyId().value(), "access_key");
      EXPECT_EQ(result->secretAccessKey().value(),
                "secret_key");
      EXPECT_EQ(result->sessionToken().value(), "session_token");
    }));
  
  EXPECT_EQ(nullptr, config.getCredentials(ext_config_2, &callbacks_2));
}

TEST_F(ConfigTest, WithStsCreds) {

  prepareSTS();
  
  envoy::config::filter::http::aws_lambda::v2::AWSLambdaProtocolExtension
      protoextconfig;
  protoextconfig.set_role_arn("role_arn");

  auto cred_provider = std::make_unique<
      NiceMock<Envoy::Extensions::Common::Aws::MockCredentialsProvider>>();

  auto sts_cred_provider = std::make_shared<
      NiceMock<MockStsCredentialsProvider>>();

  EXPECT_CALL(sts_factory_, create(_)).
    WillOnce(Return(sts_cred_provider));

  AWSLambdaConfigImpl config(std::move(cred_provider), context_.cluster_manager_,  sts_factory_, context_.dispatcher_,
                             context_.thread_local_, "prefix.", stats_, context_.api_,
                             protoconfig);

  NiceMock<MockStsCallbacks> callbacks;


  std::shared_ptr<const AWSLambdaProtocolExtensionConfig> ext_config = std::make_shared<const AWSLambdaProtocolExtensionConfig>(protoextconfig);
    
  EXPECT_CALL(*sts_cred_provider, find(_, _)).
    WillOnce(Invoke([&](absl::optional<std::string> role_arn_arg, ContextSharedPtr) -> void {
      EXPECT_EQ(ext_config->roleArn().value(), role_arn_arg);
    }));
  auto ptr = config.getCredentials(ext_config, &callbacks);
  EXPECT_NE(nullptr, ptr.get());
}

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
