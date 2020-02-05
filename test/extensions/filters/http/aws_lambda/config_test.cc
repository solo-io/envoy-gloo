#include "extensions/filters/http/aws_lambda/config.h"

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

class ConfigTest : public testing::Test {
public:
  ConfigTest() {}

protected:
  void SetUp() override {}

  NiceMock<Server::Configuration::MockFactoryContext> context_;
  Stats::IsolatedStoreImpl stats_;

  envoy::config::filter::http::aws_lambda::v2::AWSLambdaConfig protoconfig;

  NiceMock<Event::MockTimer> *prpareTimer() {
    NiceMock<Event::MockTimer> *timer =
        new NiceMock<Event::MockTimer>(&context_.dispatcher_);
    protoconfig.mutable_use_default_credentials()->set_value(true);
    EXPECT_CALL(context_.thread_local_, allocateSlot()).Times(1);
    return timer;
  }
};

TEST_F(ConfigTest, WithUseDefaultCreds) {
  auto timer = prpareTimer();

  const Envoy::Extensions::HttpFilters::Common::Aws::Credentials creds(
      "access_key", "secret_key");

  const Envoy::Extensions::HttpFilters::Common::Aws::Credentials creds2(
      "access_key2", "secret_key2");

  auto cred_provider = std::make_unique<NiceMock<
      Envoy::Extensions::HttpFilters::Common::Aws::MockCredentialsProvider>>();
  EXPECT_CALL(*cred_provider, getCredentials())
      .WillOnce(Return(creds))
      .WillOnce(Return(creds2));

  AWSLambdaConfigImpl config(std::move(cred_provider), context_.dispatcher_,
                             context_.thread_local_, "prefix.", stats_,
                             protoconfig);

  EXPECT_EQ(creds, *config.getCredentials());

  timer->invokeCallback();
  EXPECT_EQ(creds2, *config.getCredentials());

  EXPECT_EQ(2UL, stats_.counter("prefix.aws_lambda.fetch_success").value());
  EXPECT_EQ(2UL, stats_.counter("prefix.aws_lambda.creds_rotated").value());
  EXPECT_EQ(1UL, stats_
                     .gauge("prefix.aws_lambda.current_state",
                            Stats::Gauge::ImportMode::NeverImport)
                     .value());
  EXPECT_EQ(0UL, stats_.counter("prefix.aws_lambda.fetch_failed").value());
}

TEST_F(ConfigTest, FailingToRotate) {
  auto timer = prpareTimer();

  const Envoy::Extensions::HttpFilters::Common::Aws::Credentials creds(
      "access_key", "secret_key");

  auto cred_provider = std::make_unique<NiceMock<
      Envoy::Extensions::HttpFilters::Common::Aws::MockCredentialsProvider>>();
  EXPECT_CALL(*cred_provider, getCredentials())
      .WillOnce(Return(creds))
      .WillOnce(
          Return(Envoy::Extensions::HttpFilters::Common::Aws::Credentials()));

  AWSLambdaConfigImpl config(std::move(cred_provider), context_.dispatcher_,
                             context_.thread_local_, "prefix.", stats_,
                             protoconfig);

  EXPECT_EQ(creds, *config.getCredentials());

  timer->invokeCallback();

  // When we fail to rotate we latch to the last good credentials
  EXPECT_EQ(creds, *config.getCredentials());

  EXPECT_EQ(1UL, stats_.counter("prefix.aws_lambda.fetch_success").value());
  EXPECT_EQ(1UL, stats_.counter("prefix.aws_lambda.creds_rotated").value());
  EXPECT_EQ(0UL, stats_
                     .gauge("prefix.aws_lambda.current_state",
                            Stats::Gauge::ImportMode::NeverImport)
                     .value());
  EXPECT_EQ(1UL, stats_.counter("prefix.aws_lambda.fetch_failed").value());
}

TEST_F(ConfigTest, SameCredsOnTimer) {
  auto timer = prpareTimer();

  const Envoy::Extensions::HttpFilters::Common::Aws::Credentials creds(
      "access_key", "secret_key");

  auto cred_provider = std::make_unique<NiceMock<
      Envoy::Extensions::HttpFilters::Common::Aws::MockCredentialsProvider>>();
  EXPECT_CALL(*cred_provider, getCredentials())
      .WillOnce(Return(creds))
      .WillOnce(Return(creds));

  AWSLambdaConfigImpl config(std::move(cred_provider), context_.dispatcher_,
                             context_.thread_local_, "prefix.", stats_,
                             protoconfig);

  EXPECT_EQ(creds, *config.getCredentials());

  timer->invokeCallback();
  EXPECT_EQ(creds, *config.getCredentials());

  EXPECT_EQ(2UL, stats_.counter("prefix.aws_lambda.fetch_success").value());
  EXPECT_EQ(1UL, stats_.counter("prefix.aws_lambda.creds_rotated").value());
  EXPECT_EQ(1UL, stats_
                     .gauge("prefix.aws_lambda.current_state",
                            Stats::Gauge::ImportMode::NeverImport)
                     .value());
  EXPECT_EQ(0UL, stats_.counter("prefix.aws_lambda.fetch_failed").value());
}

TEST_F(ConfigTest, WithoutUseDefaultCreds) {
  protoconfig.mutable_use_default_credentials()->set_value(false);
  EXPECT_CALL(context_.thread_local_, allocateSlot()).Times(0);
  EXPECT_CALL(context_.dispatcher_, createTimer_(_)).Times(0);

  auto cred_provider = std::make_unique<NiceMock<
      Envoy::Extensions::HttpFilters::Common::Aws::MockCredentialsProvider>>();
  EXPECT_CALL(*cred_provider, getCredentials()).Times(0);

  AWSLambdaConfigImpl config(std::move(cred_provider), context_.dispatcher_,
                             context_.thread_local_, "prefix.", stats_,
                             protoconfig);

  EXPECT_EQ(nullptr, config.getCredentials());
}

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
