#include <chrono>
#include <thread>

#include "envoy/config/core/v3/http_uri.pb.h"

#include "source/common/http/message_impl.h"
#include "source/common/protobuf/utility.h"

#include "source/extensions/filters/http/aws_lambda/sts_credentials_provider.h"

#include "test/extensions/filters/http/aws_lambda/mocks.h"
#include "test/extensions/filters/http/common/mock.h"
#include "test/mocks/api/mocks.h"
#include "test/mocks/http/mocks.h"
#include "test/mocks/server/factory_context.h"
#include "test/mocks/thread_local/mocks.h"
#include "test/test_common/simulated_time_system.h"
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

// const std::string valid_access, valid_secret, valid_session, valid_expiration = R"(
// <AssumeRoleWithWebIdentityResponse xmlns="https://sts.amazonaws.com/doc/2011-06-15/">
//   <AssumeRoleWithWebIdentityResult>
//     <Credentials>
//       <AccessKeyId>some_access_key</AccessKeyId>
//       <SecretAccessKey>some_secret_key</SecretAccessKey>
//       <SessionToken>some_session_token</SessionToken>
//       <Expiration>2100-07-28T21:20:25Z</Expiration>
//     </Credentials>
//   </AssumeRoleWithWebIdentityResult>
// </AssumeRoleWithWebIdentityResponse>
// )";

const std::string valid_access = "some_access_key";
const std::string valid_secret = "some_secret_key";
const std::string valid_session = "some_session_token";
const std::string valid_expiration = "2100-07-28T21:20:25Z";

const std::string service_account_credentials_config = R"(
cluster: test
uri: https://foo.com
timeout: 1s
)";

class StsConnectionPoolTest : public testing::Test,
                              public Event::TestUsingSimulatedTime {
public:
  void SetUp() override {
    TestUtility::loadFromYaml(service_account_credentials_config, uri_);
    sts_fetcher_ = new testing::NiceMock<MockStsFetcher>();
  }

  HttpUri uri_;
  testing::NiceMock<Server::Configuration::MockFactoryContext>
      mock_factory_ctx_;
  testing::NiceMock<MockStsFetcher> *sts_fetcher_;
};

TEST_F(StsConnectionPoolTest, TestSuccessfulCallback) {
  std::string role_arn = "test";
  std::string web_token = "token";
  testing::NiceMock<MockStsContextCallbacks> ctx_callbacks;
  testing::NiceMock<MockStsConnectionPoolCallbacks> pool_callbacks;

  std::unique_ptr<testing::NiceMock<MockStsFetcher>> unique_fetcher{
      sts_fetcher_};
  auto sts_conn_pool = StsConnectionPool::create(
      mock_factory_ctx_.api_, mock_factory_ctx_.dispatcher_, role_arn,
      &pool_callbacks, std::move(unique_fetcher));

  // Fetch credentials first call as they are not in the cache
  EXPECT_CALL(*sts_fetcher_, fetch(_, _, _, _))
      .WillOnce(Invoke([&](const envoy::config::core::v3::HttpUri &,
                           const absl::string_view, const absl::string_view,
                           StsFetcher::Callbacks *callbacks) -> void {
        // Check that context callback is made correctly
        EXPECT_CALL(ctx_callbacks, onSuccess(_))
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
        // Check that credentials provider callback is made correctly
        EXPECT_CALL(pool_callbacks, onResult(_, _))
            .WillOnce(Invoke([&](std::shared_ptr<const StsCredentials> result,
                                 std::string_view inner_role_arn) -> void {
              EXPECT_EQ(result->accessKeyId().value(), "some_access_key");
              EXPECT_EQ(result->secretAccessKey().value(), "some_secret_key");
              EXPECT_EQ(result->sessionToken().value(), "some_session_token");
              EXPECT_EQ(role_arn, inner_role_arn);
            }));

        callbacks->onSuccess(valid_access, valid_secret, valid_session, valid_expiration);
      }));

  sts_conn_pool->add(&ctx_callbacks);

  sts_conn_pool->init(uri_, web_token);
}

TEST_F(StsConnectionPoolTest, TestPostInitAdd) {
  std::string role_arn = "test";
  std::string web_token = "token";
  testing::NiceMock<MockStsContextCallbacks> ctx_callbacks;
  testing::NiceMock<MockStsConnectionPoolCallbacks> pool_callbacks;

  std::unique_ptr<testing::NiceMock<MockStsFetcher>> unique_fetcher{
      sts_fetcher_};
  auto sts_conn_pool = StsConnectionPool::create(
      mock_factory_ctx_.api_, mock_factory_ctx_.dispatcher_, role_arn,
      &pool_callbacks, std::move(unique_fetcher));

  StsFetcher::Callbacks *lambda_callbacks;
  // Fetch credentials first call as they are not in the cache
  EXPECT_CALL(*sts_fetcher_, fetch(_, _, _, _))
      .WillOnce(Invoke([&](const envoy::config::core::v3::HttpUri &,
                           const absl::string_view, const absl::string_view,
                           StsFetcher::Callbacks *callbacks) -> void {
        lambda_callbacks = callbacks;
      }));

  sts_conn_pool->add(&ctx_callbacks);

  sts_conn_pool->init(uri_, web_token);

  auto context_1 = sts_conn_pool->add(&ctx_callbacks);

  // Expect the context to be removed
  EXPECT_CALL(mock_factory_ctx_.dispatcher_, deferredDelete_(_));

  context_1->cancel();

  sts_conn_pool->add(&ctx_callbacks);
  // Check that context callback is made correctly
  EXPECT_CALL(ctx_callbacks, onSuccess(_))
      .Times(2)
      .WillRepeatedly(Invoke(
          [&](std::shared_ptr<const Envoy::Extensions::Common::Aws::Credentials>
                  result) -> void {
            EXPECT_EQ(result->accessKeyId().value(), "some_access_key");
            EXPECT_EQ(result->secretAccessKey().value(), "some_secret_key");
            EXPECT_EQ(result->sessionToken().value(), "some_session_token");
          }));
  // Check that credentials provider callback is made correctly
  EXPECT_CALL(pool_callbacks, onResult(_, _))
      .WillOnce(Invoke([&](std::shared_ptr<const StsCredentials> result,
                           std::string_view inner_role_arn) -> void {
        EXPECT_EQ(result->accessKeyId().value(), "some_access_key");
        EXPECT_EQ(result->secretAccessKey().value(), "some_secret_key");
        EXPECT_EQ(result->sessionToken().value(), "some_session_token");
        EXPECT_EQ(role_arn, inner_role_arn);
      }));

  lambda_callbacks->onSuccess(valid_access, valid_secret, valid_session, valid_expiration);
}

TEST_F(StsConnectionPoolTest, TestFailure) {
  // Setup
  std::string role_arn = "test";
  std::string web_token = "token";
  testing::NiceMock<MockStsContextCallbacks> ctx_callbacks;
  testing::NiceMock<MockStsConnectionPoolCallbacks> pool_callbacks;

  std::unique_ptr<testing::NiceMock<MockStsFetcher>> unique_fetcher{
      sts_fetcher_};
  auto sts_conn_pool = StsConnectionPool::create(
      mock_factory_ctx_.api_, mock_factory_ctx_.dispatcher_, role_arn,
      &pool_callbacks, std::move(unique_fetcher));

  // Fetch credentials first call as they are not in the cache
  EXPECT_CALL(*sts_fetcher_, fetch(_, _, _, _))
      .WillOnce(Invoke([&](const envoy::config::core::v3::HttpUri &,
                           const absl::string_view, const absl::string_view,
                           StsFetcher::Callbacks *callbacks) -> void {
        // Check that context callback is made correctly
        EXPECT_CALL(ctx_callbacks, onFailure(_))
            .Times(2)
            .WillRepeatedly(
                Invoke([&](CredentialsFailureStatus reason) -> void {
                  EXPECT_EQ(reason, CredentialsFailureStatus::InvalidSts);
                }));

        callbacks->onFailure(CredentialsFailureStatus::InvalidSts);
      }));

  sts_conn_pool->add(&ctx_callbacks);

  sts_conn_pool->add(&ctx_callbacks);

  sts_conn_pool->init(uri_, web_token);
}

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
