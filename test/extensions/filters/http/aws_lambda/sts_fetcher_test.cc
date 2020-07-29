#include <chrono>
#include <thread>

#include "envoy/config/core/v3/http_uri.pb.h"

#include "common/http/message_impl.h"
#include "common/protobuf/utility.h"

#include "extensions/filters/http/aws_lambda/sts_fetcher.h"

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
using Envoy::Extensions::HttpFilters::Common;

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {
namespace {

const char validResponse = R"(
<AssumeRoleWithWebIdentityResponse xmlns="https://sts.amazonaws.com/doc/2011-06-15/">
  <AssumeRoleWithWebIdentityResult>
    <Credentials>
      <AccessKeyId>some_access_key</AccessKeyId>
      <SecretAccessKey>some_secret_key</SecretAccessKey>
      <SessionToken>some_web_token</SessionToken>
      <Expiration>2020-07-28T21:20:25Z</Expiration>
    </Credentials>
  </AssumeRoleWithWebIdentityResult>
</AssumeRoleWithWebIdentityResponse>
)";

const std::string JwksUri = R"(
uri: https://sts_server/sts_path
cluster: sts_cluster
timeout:
  seconds: 5
)";

class StsFetcherTest : public testing::Test {
public:
  void SetUp() override { TestUtility::loadFromYaml(JwksUri, uri_); }
  HttpUri uri_;
  testing::NiceMock<Server::Configuration::MockFactoryContext> mock_factory_ctx_;
  NiceMock<Tracing::MockSpan> parent_span_;
};

// Test findByIssuer
TEST_F(StsFetcherTest, TestGetSuccess) {
  // Setup
  MockUpstream mock_sts(mock_factory_ctx_.cluster_manager_, "200", publicKey);
  MockJwksReceiver receiver;
  std::unique_ptr<StsFetcher> fetcher(StsFetcher::create(mock_factory_ctx_.cluster_manager_));
  EXPECT_TRUE(fetcher != nullptr);
  EXPECT_CALL(receiver, onJwksSuccessImpl(testing::_)).Times(1);
  EXPECT_CALL(receiver, onJwksError(testing::_)).Times(0);

  // Act
  fetcher->fetch(uri_, parent_span_, receiver);
}

TEST_F(StsFetcherTest, TestGet400) {
  // Setup
  MockUpstream mock_sts(mock_factory_ctx_.cluster_manager_, "400", "invalid");
  MockJwksReceiver receiver;
  std::unique_ptr<StsFetcher> fetcher(StsFetcher::create(mock_factory_ctx_.cluster_manager_));
  EXPECT_TRUE(fetcher != nullptr);
  EXPECT_CALL(receiver, onJwksSuccessImpl(testing::_)).Times(0);
  EXPECT_CALL(receiver, onJwksError(StsFetcher::JwksReceiver::Failure::Network)).Times(1);

  // Act
  fetcher->fetch(uri_, parent_span_, receiver);
}

TEST_F(StsFetcherTest, TestGetNoBody) {
  // Setup
  MockUpstream mock_sts(mock_factory_ctx_.cluster_manager_, "200", "");
  MockJwksReceiver receiver;
  std::unique_ptr<StsFetcher> fetcher(StsFetcher::create(mock_factory_ctx_.cluster_manager_));
  EXPECT_TRUE(fetcher != nullptr);
  EXPECT_CALL(receiver, onJwksSuccessImpl(testing::_)).Times(0);
  EXPECT_CALL(receiver, onJwksError(StsFetcher::JwksReceiver::Failure::Network)).Times(1);

  // Act
  fetcher->fetch(uri_, parent_span_, receiver);
}

TEST_F(StsFetcherTest, TestGetInvalidJwks) {
  // Setup
  MockUpstream mock_sts(mock_factory_ctx_.cluster_manager_, "200", "invalid");
  MockJwksReceiver receiver;
  std::unique_ptr<StsFetcher> fetcher(StsFetcher::create(mock_factory_ctx_.cluster_manager_));
  EXPECT_TRUE(fetcher != nullptr);
  EXPECT_CALL(receiver, onJwksSuccessImpl(testing::_)).Times(0);
  EXPECT_CALL(receiver, onJwksError(StsFetcher::JwksReceiver::Failure::InvalidJwks)).Times(1);

  // Act
  fetcher->fetch(uri_, parent_span_, receiver);
}

TEST_F(StsFetcherTest, TestHttpFailure) {
  // Setup
  MockUpstream mock_sts(mock_factory_ctx_.cluster_manager_,
                           Http::AsyncClient::FailureReason::Reset);
  MockJwksReceiver receiver;
  std::unique_ptr<StsFetcher> fetcher(StsFetcher::create(mock_factory_ctx_.cluster_manager_));
  EXPECT_TRUE(fetcher != nullptr);
  EXPECT_CALL(receiver, onJwksSuccessImpl(testing::_)).Times(0);
  EXPECT_CALL(receiver, onJwksError(StsFetcher::JwksReceiver::Failure::Network)).Times(1);

  // Act
  fetcher->fetch(uri_, parent_span_, receiver);
}

TEST_F(StsFetcherTest, TestCancel) {
  // Setup
  Http::MockAsyncClientRequest request(&(mock_factory_ctx_.cluster_manager_.async_client_));
  MockUpstream mock_sts(mock_factory_ctx_.cluster_manager_, &request);
  MockJwksReceiver receiver;
  std::unique_ptr<StsFetcher> fetcher(StsFetcher::create(mock_factory_ctx_.cluster_manager_));
  EXPECT_TRUE(fetcher != nullptr);
  EXPECT_CALL(request, cancel()).Times(1);
  EXPECT_CALL(receiver, onJwksSuccessImpl(testing::_)).Times(0);
  EXPECT_CALL(receiver, onJwksError(testing::_)).Times(0);

  // Act
  fetcher->fetch(uri_, parent_span_, receiver);
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
