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

/*
time since epoch. to change it, do this in python:

from datetime import datetime, timezone
d = datetime.strptime('2100-07-28T21:20:25Z', '%Y-%m-%dT%H:%M:%SZ')
d = d.replace(tzinfo=timezone.utc)
d.timestamp()
*/
std::chrono::seconds expiry_time(4120492825);

const std::string service_account_credentials_config = R"(
cluster: test
uri: https://foo.com
timeout: 1s
)";

class StsCredentialsProviderTest : public testing::Test,
                                   public Event::TestUsingSimulatedTime {
public:
  void SetUp() override {
    TestUtility::loadFromYaml(service_account_credentials_config, config_);

    sts_connection_pool_ = std::make_unique<testing::NiceMock<MockStsConnectionPool>>();
    sts_chained_connection_pool_ =
         std::make_unique<testing::NiceMock<MockStsConnectionPool>>();
    sts_connection_pool_factory_ =
        std::make_unique<testing::NiceMock<MockStsConnectionPoolFactory>>();
  }

  envoy::config::filter::http::aws_lambda::v2::
      AWSLambdaConfig_ServiceAccountCredentials config_;
  testing::NiceMock<Server::Configuration::MockFactoryContext>
      mock_factory_ctx_;
  std::unique_ptr<testing::NiceMock<MockStsConnectionPoolFactory>> sts_connection_pool_factory_;
  std::unique_ptr<testing::NiceMock<MockStsConnectionPool>> sts_connection_pool_;
  std::unique_ptr<testing::NiceMock<MockStsConnectionPool>> sts_chained_connection_pool_;
};

TEST_F(StsCredentialsProviderTest, TestFullFlow) {
  // Setup
  std::string role_arn = "test_arn";
  std::string token = "test_token";
  std::unique_ptr<testing::NiceMock<MockStsConnectionPoolFactory>> factory_ = std::move(sts_connection_pool_factory_);
  auto* factory = factory_.get();
  auto sts_provider = StsCredentialsProvider::create(
      config_, mock_factory_ctx_.api_, mock_factory_ctx_.cluster_manager_,
      std::move(factory_), token, role_arn);
  testing::NiceMock<MockStsContextCallbacks> ctx_callbacks_1;

  std::unique_ptr<testing::NiceMock<MockStsConnectionPool>> unique_pool = std::move(sts_connection_pool_);
  auto* sts_connection_pool = unique_pool.get();
  StsConnectionPool::Callbacks *credentials_provider_callbacks;

  EXPECT_CALL(*factory, build(_, _, _, _))
      .WillOnce(Invoke([&](const absl::string_view cache_lookup_arg,
                           const absl::string_view role_arn_arg,
                           StsConnectionPool::Callbacks *callbacks,
                           StsFetcherPtr) -> StsConnectionPoolPtr {
        EXPECT_EQ(role_arn_arg, role_arn);
        EXPECT_EQ(cache_lookup_arg, role_arn);
        credentials_provider_callbacks = callbacks;
        return std::move(unique_pool);
      }));

  EXPECT_CALL(*sts_connection_pool, init(_, _, _))
      .WillOnce(Invoke([&](const envoy::config::core::v3::HttpUri &uri,
                           const absl::string_view web_token,
                           StsCredentialsConstSharedPtr ) {
        EXPECT_EQ(web_token, token);
        EXPECT_EQ(uri.uri(), config_.uri());
        EXPECT_EQ(uri.cluster(), config_.cluster());
      }));
  EXPECT_CALL(*sts_connection_pool, add(_));

  sts_provider->find(role_arn, false, &ctx_callbacks_1);

  testing::NiceMock<MockStsContextCallbacks> ctx_callbacks_2;

  EXPECT_CALL(*sts_connection_pool, requestInFlight()).WillOnce(Return(true));
  EXPECT_CALL(*sts_connection_pool, add(_));

  sts_provider->find(role_arn, false, &ctx_callbacks_2);

  // place credentials in the cache
  auto credentials = std::make_shared<const StsCredentials>(
      "access_key", "secret_key", "session_token",
      SystemTime(expiry_time - std::chrono::minutes(5)));
  std::list<std::string> to_chain;
  credentials_provider_callbacks->onResult(credentials, role_arn, to_chain);

  testing::NiceMock<MockStsContextCallbacks> ctx_callbacks_3;
  EXPECT_CALL(ctx_callbacks_3, onSuccess(_))
      .WillOnce(Invoke(
          [&](std::shared_ptr<const Envoy::Extensions::Common::Aws::Credentials>
                  success_creds) {
            EXPECT_EQ(success_creds->accessKeyId(), "access_key");
            EXPECT_EQ(success_creds->secretAccessKey(), "secret_key");
            EXPECT_EQ(success_creds->sessionToken(), "session_token");
          }));
  sts_provider->find(role_arn, false, &ctx_callbacks_3);

  // overwrite the original creds
  testing::NiceMock<MockStsContextCallbacks> ctx_callbacks_4;
  auto credentials2 = std::make_shared<const StsCredentials>(
    "access_key2", "secret_key2", "session_token2",
    SystemTime(expiry_time - std::chrono::minutes(5)));
  credentials_provider_callbacks->onResult(credentials2, role_arn, to_chain);

    EXPECT_CALL(ctx_callbacks_4, onSuccess(_))
      .WillOnce(Invoke(
          [&](std::shared_ptr<const Envoy::Extensions::Common::Aws::Credentials>
                  success_creds) {
            EXPECT_EQ(success_creds->accessKeyId(), "access_key2");
            EXPECT_EQ(success_creds->secretAccessKey(), "secret_key2");
            EXPECT_EQ(success_creds->sessionToken(), "session_token2");
          }));
  sts_provider->find(role_arn, false, &ctx_callbacks_4);
  
}

TEST_F(StsCredentialsProviderTest, TestFullChainedFlow) {
  // Setup
  std::string base_role_arn = "test_arn";
  std::string role_arn = "test_arn_chained";
  std::string token = "test_token";
  std::unique_ptr<testing::NiceMock<MockStsConnectionPoolFactory>> factory_ = std::move(sts_connection_pool_factory_);
  auto* factory = factory_.get();
  auto sts_provider = StsCredentialsProvider::create(
      config_, mock_factory_ctx_.api_, mock_factory_ctx_.cluster_manager_,
      std::move(factory_), token, base_role_arn);
  testing::NiceMock<MockStsContextCallbacks> ctx_callbacks_1;

  std::unique_ptr<testing::NiceMock<MockStsConnectionPool>> unique_pool = std::move(sts_connection_pool_);
  auto* sts_connection_pool = unique_pool.get();
  std::unique_ptr<testing::NiceMock<MockStsConnectionPool>> unique_chained_pool =std::move(sts_chained_connection_pool_);
  auto* chained_pool = unique_chained_pool.get();
  StsConnectionPool::Callbacks *credentials_provider_callbacks;
  StsConnectionPool::Callbacks *credentials_provider_callbacks_chained;
  
  // Expect to see the chained pool created first and then the base pool
  EXPECT_CALL(*factory, build(_, _, _, _))
      .WillOnce(Invoke([&](const absl::string_view cache_lookup_arg,
                           const absl::string_view role_arn_arg,
                           StsConnectionPool::Callbacks *callbacks,
                           StsFetcherPtr) -> StsConnectionPoolPtr {
        EXPECT_EQ(role_arn_arg, role_arn);
        EXPECT_EQ(cache_lookup_arg, role_arn);
        credentials_provider_callbacks_chained = callbacks;
        return std::move(unique_chained_pool);
      })).WillOnce(Invoke([&](const absl::string_view cache_lookup_arg,
                           const absl::string_view role_arn_arg,
                           StsConnectionPool::Callbacks *callbacks,
                           StsFetcherPtr) -> StsConnectionPoolPtr {
        EXPECT_EQ(role_arn_arg, base_role_arn);
        EXPECT_EQ(cache_lookup_arg, base_role_arn);
        credentials_provider_callbacks = callbacks;
        return std::move(unique_pool);
      }));
    
  // expect the base pool to be initialized with fetch
  EXPECT_CALL(*sts_connection_pool, init(_, _, _))
      .WillOnce(Invoke([&](const envoy::config::core::v3::HttpUri &uri,
                           const absl::string_view web_token,
                           StsCredentialsConstSharedPtr ) {
        EXPECT_EQ(web_token, token);
        EXPECT_EQ(uri.uri(), config_.uri());
        EXPECT_EQ(uri.cluster(), config_.cluster());
      }));

  EXPECT_CALL(*chained_pool, setInFlight());
  EXPECT_CALL(*chained_pool, add(_));

  sts_provider->find(role_arn, false, &ctx_callbacks_1);

  testing::NiceMock<MockStsContextCallbacks> ctx_callbacks_2;

  EXPECT_CALL(*chained_pool,
    requestInFlight()).WillOnce(Return(true));
  EXPECT_CALL(*chained_pool, add(_));
  sts_provider->find(role_arn, false, &ctx_callbacks_2);

  testing::NiceMock<MockStsContextCallbacks> ctx_callbacks_3;
  EXPECT_CALL(*sts_connection_pool, requestInFlight()).WillOnce(Return(true));
  EXPECT_CALL(*sts_connection_pool, add(_));
  sts_provider->find(base_role_arn, false, &ctx_callbacks_3);

  // place credentials in the cache
  auto credentials = std::make_shared<const StsCredentials>(
      "access_key", "secret_key", "session_token",
      SystemTime(expiry_time - std::chrono::minutes(5)));
  std::list<std::string> to_chain;
  credentials_provider_callbacks->onResult(credentials, role_arn, to_chain);

  testing::NiceMock<MockStsContextCallbacks> ctx_callbacks_4;
  EXPECT_CALL(ctx_callbacks_4, onSuccess(_))
      .WillOnce(Invoke(
          [&](std::shared_ptr<const Envoy::Extensions::Common::Aws::Credentials>
                  success_creds) {
            EXPECT_EQ(success_creds->accessKeyId(), "access_key");
            EXPECT_EQ(success_creds->secretAccessKey(), "secret_key");
            EXPECT_EQ(success_creds->sessionToken(), "session_token");
          }));
  sts_provider->find(role_arn, false, &ctx_callbacks_4);
}

TEST_F(StsCredentialsProviderTest, TestUnchainedFlow) {
  // Setup
  std::string role_arn = "test_arn";
  std::string cache_arn = "no-chain-test_arn";
  std::string token = "test_token";
  std::unique_ptr<testing::NiceMock<MockStsConnectionPoolFactory>> factory_ = std::move(
      sts_connection_pool_factory_);
  auto* factory = factory_.get();

  auto sts_provider = StsCredentialsProvider::create(
      config_, mock_factory_ctx_.api_, mock_factory_ctx_.cluster_manager_,
      std::move(factory_), token, role_arn);
  testing::NiceMock<MockStsContextCallbacks> ctx_callbacks_1;

  std::unique_ptr<testing::NiceMock<MockStsConnectionPool>> unique_pool = std::move(
      sts_connection_pool_);
  auto* sts_connection_pool = unique_pool.get();
  StsConnectionPool::Callbacks *credentials_provider_callbacks;

  EXPECT_CALL(*factory, build(_, _, _, _))
      .WillOnce(Invoke([&](const absl::string_view cache_lookup_role,
                           const absl::string_view role_arn_arg,
                           StsConnectionPool::Callbacks *callbacks,
                           StsFetcherPtr) -> StsConnectionPoolPtr {
        EXPECT_EQ(role_arn_arg, role_arn);
        EXPECT_EQ(cache_lookup_role, cache_arn);

        credentials_provider_callbacks = callbacks;
        return std::move(unique_pool);
      }));

  EXPECT_CALL(*sts_connection_pool, init(_, _, _))
      .WillOnce(Invoke([&](const envoy::config::core::v3::HttpUri &uri,
                           const absl::string_view web_token,
                           StsCredentialsConstSharedPtr ) {
        EXPECT_EQ(web_token, token);
        EXPECT_EQ(uri.uri(), config_.uri());
        EXPECT_EQ(uri.cluster(), config_.cluster());
      }));
  EXPECT_CALL(*sts_connection_pool, add(_));

  sts_provider->find(role_arn, true, &ctx_callbacks_1);

  testing::NiceMock<MockStsContextCallbacks> ctx_callbacks_2;

  EXPECT_CALL(*sts_connection_pool, requestInFlight()).WillOnce(Return(true));
  EXPECT_CALL(*sts_connection_pool, add(_));

  sts_provider->find(role_arn, true, &ctx_callbacks_2);

  // place credentials in the cache
  auto credentials = std::make_shared<const StsCredentials>(
      "access_key", "secret_key", "session_token",
      SystemTime(expiry_time - std::chrono::minutes(5)));
  std::list<std::string> to_chain;
  credentials_provider_callbacks->onResult(credentials, cache_arn, to_chain);

  testing::NiceMock<MockStsContextCallbacks> ctx_callbacks_3;
  EXPECT_CALL(ctx_callbacks_3, onSuccess(_))
      .WillOnce(Invoke(
          [&](std::shared_ptr<const Envoy::Extensions::Common::Aws::Credentials>
                  success_creds) {
            EXPECT_EQ(success_creds->accessKeyId(), "access_key");
            EXPECT_EQ(success_creds->secretAccessKey(), "secret_key");
            EXPECT_EQ(success_creds->sessionToken(), "session_token");
          }));
  sts_provider->find(role_arn, true, &ctx_callbacks_3);
}

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
