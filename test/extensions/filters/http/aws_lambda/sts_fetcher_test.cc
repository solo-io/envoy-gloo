#include <chrono>
#include <thread>

#include "envoy/config/core/v3/http_uri.pb.h"

#include "common/http/message_impl.h"
#include "common/protobuf/utility.h"

#include "extensions/filters/http/aws_lambda/sts_fetcher.h"

#include "test/extensions/filters/http/aws_lambda/mocks.h"
#include "test/mocks/server/factory_context.h"
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
namespace {

const std::string valid_response = R"(
<AssumeRoleWithWebIdentityResponse xmlns="https://sts.amazonaws.com/doc/2011-06-15/">
  <AssumeRoleWithWebIdentityResult>
    <Credentials>
      <AccessKeyId>some_access_key</AccessKeyId>
      <SecretAccessKey>some_secret_key</SecretAccessKey>
      <SessionToken>some_session_token</SessionToken>
      <Expiration>3000-07-28T21:20:25Z</Expiration>
    </Credentials>
  </AssumeRoleWithWebIdentityResult>
</AssumeRoleWithWebIdentityResponse>
)";

const std::string expired_token_response = R"(
<ErrorResponse xmlns="http://webservices.amazon.com/AWSFault/2005-15-09">
  <Error>
    <Type>Sender</Type>
    <Code>ExpiredTokenException</Code>
    <Message>token is expired</Message>
  </Error>
</ErrorResponse>
)";

const std::string service_account_credentials_config = R"(
cluster: test
uri: https://foo.com
timeout: 1s
)";

const std::string role_arn = "role_arn";
const std::string web_token = "web_token";

class StsFetcherTest : public testing::Test {
public:
  void SetUp() override {
    TestUtility::loadFromYaml(service_account_credentials_config, uri_);
  }
  HttpUri uri_;
  testing::NiceMock<Server::Configuration::MockFactoryContext>
      mock_factory_ctx_;
};

// Test findByIssuer
TEST_F(StsFetcherTest, TestGetSuccess) {
  // Setup
  MockUpstream mock_sts(mock_factory_ctx_.cluster_manager_, "200",
                        valid_response);
  std::unique_ptr<StsFetcher> fetcher(StsFetcher::create(
      mock_factory_ctx_.cluster_manager_, mock_factory_ctx_.api_));
  EXPECT_TRUE(fetcher != nullptr);

  testing::NiceMock<MockStsFetcherCallbacks> callbacks;
  EXPECT_CALL(callbacks, onSuccess(valid_response)).Times(1);
  // Act
  fetcher->fetch(uri_, role_arn, web_token, &callbacks);
}

TEST_F(StsFetcherTest, TestGet503) {
  // Setup
  MockUpstream mock_sts(mock_factory_ctx_.cluster_manager_, "503", "invalid");
  std::unique_ptr<StsFetcher> fetcher(StsFetcher::create(
      mock_factory_ctx_.cluster_manager_, mock_factory_ctx_.api_));
  EXPECT_TRUE(fetcher != nullptr);

  testing::NiceMock<MockStsFetcherCallbacks> callbacks;
  EXPECT_CALL(callbacks, onFailure(CredentialsFailureStatus::Network)).Times(1);

  // Act
  fetcher->fetch(uri_, role_arn, web_token, &callbacks);
}

TEST_F(StsFetcherTest, TestCredentialsExpired) {
  // Setup
  MockUpstream mock_sts(mock_factory_ctx_.cluster_manager_, "401",
                        expired_token_response);
  std::unique_ptr<StsFetcher> fetcher(StsFetcher::create(
      mock_factory_ctx_.cluster_manager_, mock_factory_ctx_.api_));
  EXPECT_TRUE(fetcher != nullptr);

  testing::NiceMock<MockStsFetcherCallbacks> callbacks;
  EXPECT_CALL(callbacks, onFailure(CredentialsFailureStatus::ExpiredToken))
      .Times(1);

  // Act
  fetcher->fetch(uri_, role_arn, web_token, &callbacks);
}

TEST_F(StsFetcherTest, TestHttpFailure) {
  // Setup
  MockUpstream mock_sts(mock_factory_ctx_.cluster_manager_,
                        Http::AsyncClient::FailureReason::Reset);
  std::unique_ptr<StsFetcher> fetcher(StsFetcher::create(
      mock_factory_ctx_.cluster_manager_, mock_factory_ctx_.api_));
  EXPECT_TRUE(fetcher != nullptr);

  testing::NiceMock<MockStsFetcherCallbacks> callbacks;
  EXPECT_CALL(callbacks, onFailure(CredentialsFailureStatus::Network)).Times(1);

  // Act
  fetcher->fetch(uri_, role_arn, web_token, &callbacks);
}

TEST_F(StsFetcherTest, TestCancel) {
  // Setup
  Http::MockAsyncClientRequest request(
      &(mock_factory_ctx_.cluster_manager_.thread_local_cluster_.async_client_));
  MockUpstream mock_sts(mock_factory_ctx_.cluster_manager_, &request);
  std::unique_ptr<StsFetcher> fetcher(StsFetcher::create(
      mock_factory_ctx_.cluster_manager_, mock_factory_ctx_.api_));
  EXPECT_TRUE(fetcher != nullptr);
  EXPECT_CALL(request, cancel()).Times(1);

  testing::NiceMock<MockStsFetcherCallbacks> callbacks;

  // Act
  fetcher->fetch(uri_, role_arn, web_token, &callbacks);
  // Proper cancel
  fetcher->cancel();
  // Re-entrant cancel
  fetcher->cancel();
}

} // namespace
} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
