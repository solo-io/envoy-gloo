#include <chrono>
#include <thread>

#include "envoy/config/core/v3/http_uri.pb.h"

#include "common/http/message_impl.h"
#include "common/protobuf/utility.h"

#include "extensions/filters/http/aws_lambda/sts_credentials_provider.h"

#include "test/extensions/filters/http/aws_lambda/mocks.h"
#include "test/extensions/filters/http/common/mock.h"
#include "test/mocks/http/mocks.h"
#include "test/test_common/utility.h"

using testing::_;
using testing::AtLeast;
using testing::Invoke;
using testing::Return;
using testing::ReturnPointee;
using testing::ReturnRef;
using testing::SaveArg;
using testing::WithArg;

using envoy::config::core::v3::HttpUri;

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {

const std::string validResponse = R"(
<AssumeRoleWithWebIdentityResponse xmlns="https://sts.amazonaws.com/doc/2011-06-15/">
  <AssumeRoleWithWebIdentityResult>
    <Credentials>
      <AccessKeyId>some_access_key</AccessKeyId>
      <SecretAccessKey>some_secret_key</SecretAccessKey>
      <SessionToken>some_session_token</SessionToken>
      <Expiration>2020-07-28T21:20:25Z</Expiration>
    </Credentials>
  </AssumeRoleWithWebIdentityResult>
</AssumeRoleWithWebIdentityResponse>
)";

const std::string service_account_credentials_config = R"(
cluster: test
uri: https://foo.com
timeout: 1s
)";

class StsCredentialsProviderTest : public testing::Test {
public:
  void SetUp() override {
    TestUtility::loadFromYaml(service_account_credentials_config, config_);
  }

  void init() {
    EXPECT_CALL(api_.file_system_, fileExists(_))
        .Times(1)
        .WillOnce(Return(true));
    EXPECT_CALL(api_.file_system_, fileReadToEnd(_))
        .Times(1)
        .WillOnce(Return("web_token"));

    watcher_ = new Filesystem::MockWatcher();
    EXPECT_CALL(dispatcher_, createFilesystemWatcher_())
        .WillOnce(Return(watcher_));
    EXPECT_CALL(*watcher_, addWatch("test", _, _)).Times(1);

    setenv("AWS_WEB_IDENTITY_TOKEN_FILE", "test", 1);
    setenv("AWS_ROLE_ARN", "test", 1);
    sts_provider_ = StsCredentialsProviderImpl::create(
        config_, api_, thread_local_, dispatcher_);
  }

  envoy::config::filter::http::aws_lambda::v2::
      AWSLambdaConfig_ServiceAccountCredentials config_;
  testing::NiceMock<ThreadLocal::MockInstance> thread_local_;
  testing::NiceMock<Api::MockApi> api_;
  testing::NiceMock<Event::MockDispatcher> dispatcher_;
  Filesystem::MockWatcher *watcher_;
  std::shared_ptr<StsCredentialsProvider> sts_provider_;
};

// Test findByIssuer
TEST_F(StsCredentialsProviderTest, InitWithoutCrashing) {
  // Setup
  init();
}

TEST_F(StsCredentialsProviderTest, TestSuccessCallback) {
  // Setup
  init();
  absl::optional<std::string> role_arn = "test";
  std::shared_ptr<MockStsContext> context = std::make_shared<MockStsContext>();
  EXPECT_CALL(*context, fetcher()).Times(1);

  EXPECT_CALL(context->fetcher_, fetch(_, _, _, _, _))
      .WillOnce(Invoke([&](const envoy::config::core::v3::HttpUri &,
                           const absl::string_view, const absl::string_view,
                           StsFetcher::SuccessCallback success,
                           StsFetcher::FailureCallback) -> void {
        EXPECT_CALL(*context, callbacks()).Times(1);

        EXPECT_CALL(context->callbacks_, onSuccess(_))
            .WillOnce(
                Invoke([&](std::shared_ptr<
                           const Envoy::Extensions::Common::Aws::Credentials>
                               result) -> void {
                  EXPECT_EQ(result->accessKeyId().value(), "some_access_key");
                  EXPECT_EQ(result->secretAccessKey().value(),
                            "some_secret_key");
                  EXPECT_EQ(result->sessionToken().value(),
                            "some_session_token");
                }));

        success(&validResponse);
      }));

  sts_provider_->find(role_arn, context);
}

TEST_F(StsCredentialsProviderTest, TestSuccessCache) {
  // Setup
  init();
  absl::optional<std::string> role_arn = "test";
  std::shared_ptr<MockStsContext> context = std::make_shared<MockStsContext>();
  EXPECT_CALL(*context, fetcher()).Times(1);

  EXPECT_CALL(context->fetcher_, fetch(_, _, _, _, _))
      .WillOnce(Invoke([&](const envoy::config::core::v3::HttpUri &,
                           const absl::string_view, const absl::string_view,
                           StsFetcher::SuccessCallback success,
                           StsFetcher::FailureCallback) -> void {
        EXPECT_CALL(*context, callbacks()).Times(1);

        EXPECT_CALL(context->callbacks_, onSuccess(_))
            .WillOnce(
                Invoke([&](std::shared_ptr<
                           const Envoy::Extensions::Common::Aws::Credentials>
                               result) -> void {
                  EXPECT_EQ(result->accessKeyId().value(), "some_access_key");
                  EXPECT_EQ(result->secretAccessKey().value(),
                            "some_secret_key");
                  EXPECT_EQ(result->sessionToken().value(),
                            "some_session_token");
                }));

        success(&validResponse);
      }));

  sts_provider_->find(role_arn, context);
}


TEST_F(StsCredentialsProviderTest, TestFailure) {
  // Setup
  init();
  absl::optional<std::string> role_arn = "test";
  std::shared_ptr<MockStsContext> context = std::make_shared<MockStsContext>();
  EXPECT_CALL(*context, fetcher()).Times(1);

  EXPECT_CALL(context->fetcher_, fetch(_, _, _, _, _))
      .WillOnce(Invoke([&](const envoy::config::core::v3::HttpUri &,
                           const absl::string_view, const absl::string_view,
                           StsFetcher::SuccessCallback,
                           StsFetcher::FailureCallback failure) -> void {
        EXPECT_CALL(*context, callbacks()).Times(1);

        EXPECT_CALL(context->callbacks_, onFailure(_))
            .WillOnce(
                Invoke([&](CredentialsFailureStatus reason) -> void {
                  EXPECT_EQ(reason, CredentialsFailureStatus::InvalidSts);
                }));

        failure(CredentialsFailureStatus::InvalidSts);
      }));

  sts_provider_->find(role_arn, context);
}

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
