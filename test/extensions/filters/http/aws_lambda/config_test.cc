#include "extensions/filters/http/aws_lambda/config.h"

#include "test/extensions/filters/http/common/aws/mocks.h"
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

  envoy::config::filter::http::aws_lambda::v2::AWSLambdaConfig protoconfig;
};

TEST_F(ConfigTest, WithUseDefaultCreds) {
  NiceMock<Event::MockTimer> *timer =
      new NiceMock<Event::MockTimer>(&context_.dispatcher_);
  protoconfig.mutable_use_default_credentials()->set_value(true);
  EXPECT_CALL(context_.thread_local_, allocateSlot()).Times(1);
  // No need to expect a call createTimer as the mock timer does that.
  EXPECT_CALL(*timer, enableTimer(_)).Times(2);

  const Envoy::Extensions::HttpFilters::Common::Aws::Credentials creds(
      "access_key", "secret_key");

  const Envoy::Extensions::HttpFilters::Common::Aws::Credentials creds2(
      "access_key2", "secret_key2");

  auto cred_provider = std::make_unique<NiceMock<
      Envoy::Extensions::HttpFilters::Common::Aws::MockCredentialsProvider>>();
  EXPECT_CALL(*cred_provider, getCredentials())
      .WillOnce(Return(creds))
      .WillOnce(Return(creds2));

  AWSLambdaConfigImpl a(std::move(cred_provider), context_.dispatcher_,
                        context_.thread_local_, protoconfig);

  EXPECT_EQ(creds, *a.getCredentials());

  timer->invokeCallback();
  EXPECT_EQ(creds2, *a.getCredentials());
}

TEST_F(ConfigTest, WithoutUseDefaultCreds) {
  protoconfig.mutable_use_default_credentials()->set_value(false);
  EXPECT_CALL(context_.thread_local_, allocateSlot()).Times(0);
  EXPECT_CALL(context_.dispatcher_, createTimer_(_)).Times(0);

  auto cred_provider = std::make_unique<NiceMock<
      Envoy::Extensions::HttpFilters::Common::Aws::MockCredentialsProvider>>();
  EXPECT_CALL(*cred_provider, getCredentials()).Times(0);

  AWSLambdaConfigImpl a(std::move(cred_provider), context_.dispatcher_,
                        context_.thread_local_, protoconfig);

  EXPECT_EQ(nullptr, a.getCredentials());
}

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
