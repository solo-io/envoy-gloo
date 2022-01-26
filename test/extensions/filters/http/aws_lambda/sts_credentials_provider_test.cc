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

    sts_connection_pool_ = new testing::NiceMock<MockStsConnectionPool>();
    sts_chained_connection_pool_ =
         new testing::NiceMock<MockStsConnectionPool>();
    sts_connection_pool_factory_ =
        new testing::NiceMock<MockStsConnectionPoolFactory>();
  }

  envoy::config::filter::http::aws_lambda::v2::
      AWSLambdaConfig_ServiceAccountCredentials config_;
  testing::NiceMock<Server::Configuration::MockFactoryContext>
      mock_factory_ctx_;
  testing::NiceMock<MockStsConnectionPoolFactory> *sts_connection_pool_factory_;
  testing::NiceMock<MockStsConnectionPool> *sts_connection_pool_;
  testing::NiceMock<MockStsConnectionPool> *sts_chained_connection_pool_;
};

TEST_F(StsCredentialsProviderTest, TestFullFlow) {
  // Setup
  std::string role_arn = "test_arn";
  std::string token = "test_token";
  std::unique_ptr<testing::NiceMock<MockStsConnectionPoolFactory>> factory_{
      sts_connection_pool_factory_};
  auto sts_provider = StsCredentialsProvider::create(
      config_, mock_factory_ctx_.api_, mock_factory_ctx_.cluster_manager_,
      std::move(factory_), token, role_arn);
  testing::NiceMock<MockStsContextCallbacks> ctx_callbacks_1;

  std::unique_ptr<testing::NiceMock<MockStsConnectionPool>> unique_pool{
      sts_connection_pool_};
  StsConnectionPool::Callbacks *credentials_provider_callbacks;

  EXPECT_CALL(*sts_connection_pool_factory_, build(_, _, _))
      .WillOnce(Invoke([&](const absl::string_view role_arn_arg,
                           StsConnectionPool::Callbacks *callbacks,
                           StsFetcherPtr) -> StsConnectionPoolPtr {
        EXPECT_EQ(role_arn_arg, role_arn);
        credentials_provider_callbacks = callbacks;
        return std::move(unique_pool);
      }));

  EXPECT_CALL(*sts_connection_pool_, init(_, _, _))
      .WillOnce(Invoke([&](const envoy::config::core::v3::HttpUri &uri,
                           const absl::string_view web_token,
                           StsCredentialsConstSharedPtr ) {
        EXPECT_EQ(web_token, token);
        EXPECT_EQ(uri.uri(), config_.uri());
        EXPECT_EQ(uri.cluster(), config_.cluster());
      }));
  EXPECT_CALL(*sts_connection_pool_, add(_));

  sts_provider->find(role_arn, &ctx_callbacks_1);

  testing::NiceMock<MockStsContextCallbacks> ctx_callbacks_2;

  EXPECT_CALL(*sts_connection_pool_, requestInFlight()).WillOnce(Return(true));
  EXPECT_CALL(*sts_connection_pool_, add(_));

  sts_provider->find(role_arn, &ctx_callbacks_2);

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
  sts_provider->find(role_arn, &ctx_callbacks_3);
}

TEST_F(StsCredentialsProviderTest, TestFullChainedFlow) {
  // Setup
  std::string base_role_arn = "test_arn";
  std::string role_arn = "test_arn_chained";
  std::string token = "test_token";
  std::unique_ptr<testing::NiceMock<MockStsConnectionPoolFactory>> factory_{
      sts_connection_pool_factory_};
  auto sts_provider = StsCredentialsProvider::create(
      config_, mock_factory_ctx_.api_, mock_factory_ctx_.cluster_manager_,
      std::move(factory_), token, base_role_arn);
  testing::NiceMock<MockStsContextCallbacks> ctx_callbacks_1;

  std::unique_ptr<testing::NiceMock<MockStsConnectionPool>> unique_pool{
      sts_connection_pool_};
  std::unique_ptr<testing::NiceMock<MockStsConnectionPool>> unique_chained_pool{
      sts_chained_connection_pool_};
  StsConnectionPool::Callbacks *credentials_provider_callbacks;
  StsConnectionPool::Callbacks *credentials_provider_callbacks_chained;
  
  // Expect to see the chained pool created first and then the base pool
  EXPECT_CALL(*sts_connection_pool_factory_, build(_, _, _))
      .WillOnce(Invoke([&](const absl::string_view role_arn_arg,
                           StsConnectionPool::Callbacks *callbacks,
                           StsFetcherPtr) -> StsConnectionPoolPtr {
        EXPECT_EQ(role_arn_arg, role_arn);
        credentials_provider_callbacks_chained = callbacks;
        return std::move(unique_chained_pool);
      })).WillOnce(Invoke([&](const absl::string_view role_arn_arg,
                           StsConnectionPool::Callbacks *callbacks,
                           StsFetcherPtr) -> StsConnectionPoolPtr {
        EXPECT_EQ(role_arn_arg, base_role_arn);
        credentials_provider_callbacks = callbacks;
        return std::move(unique_pool);
      }));
    
  // expect the base pool to be initialized with fetch
  EXPECT_CALL(*sts_connection_pool_, init(_, _, _))
      .WillOnce(Invoke([&](const envoy::config::core::v3::HttpUri &uri,
                           const absl::string_view web_token,
                           StsCredentialsConstSharedPtr ) {
        EXPECT_EQ(web_token, token);
        EXPECT_EQ(uri.uri(), config_.uri());
        EXPECT_EQ(uri.cluster(), config_.cluster());
      }));

  EXPECT_CALL(*sts_chained_connection_pool_, setInFlight());
  EXPECT_CALL(*sts_chained_connection_pool_, add(_));

  sts_provider->find(role_arn, &ctx_callbacks_1);

  testing::NiceMock<MockStsContextCallbacks> ctx_callbacks_2;

  EXPECT_CALL(*sts_chained_connection_pool_, 
    requestInFlight()).WillOnce(Return(true));
  EXPECT_CALL(*sts_chained_connection_pool_, add(_));
  sts_provider->find(role_arn, &ctx_callbacks_2);

  testing::NiceMock<MockStsContextCallbacks> ctx_callbacks_3;
  EXPECT_CALL(*sts_connection_pool_, requestInFlight()).WillOnce(Return(true));
  EXPECT_CALL(*sts_connection_pool_, add(_));
  sts_provider->find(base_role_arn, &ctx_callbacks_3);

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
  sts_provider->find(role_arn, &ctx_callbacks_4);
}

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
