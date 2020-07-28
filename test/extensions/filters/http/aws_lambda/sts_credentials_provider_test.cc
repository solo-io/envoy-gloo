#include "extensions/filters/http/aws_lambda/sts_credentials_provider.h"

#include "test/mocks/common.h"
#include "test/mocks/server/mocks.h"
#include "test/mocks/upstream/mocks.h"
#include "test/test_common/utility.h"
#include "test/test_common/environment.h"

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

class StsCredentialsProviderTest : public testing::Test {
public:
  StsCredentialsProviderTest() {}

protected:
  void SetUp() override {}

  NiceMock<Server::Configuration::MockTransportSocketFactoryContext> context_;
  Stats::TestUtil::TestStore stats_;

  // NiceMock<Event::MockTimer> *prpareTimer() {
  //   NiceMock<Event::MockTimer> *timer =
  //       new NiceMock<Event::MockTimer>(&context_.dispatcher_);
  //   EXPECT_CALL(context_.thread_local_, allocateSlot()).Times(1);
  //   return timer;
  // }
};

TEST_F(StsCredentialsProviderTest, WithUseDefaultCreds) {
  const std::string bootstrap_path = TestEnvironment::writeStringToFileForTest(
      "web_token_file", "contents");

  TestEnvironment::setEnvVar("AWS_ROLE_ARN", "akid", 1);
  TestEnvironment::setEnvVar("AWS_WEB_IDENTITY_TOKEN_FILE", bootstrap_path, 1);
  // const Envoy::Extensions::Common::Aws::Credentials creds("access_key",
  //                                                         "secret_key");

  StsCredentialsProvider sts(context_.api(), stats_);

  EXPECT_THROW(sts.getCredentials(nullptr), EnvoyException);
}

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
