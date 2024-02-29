#pragma once

#include "envoy/config/core/v3/http_uri.pb.h"

#include "source/extensions/filters/http/aws_lambda/sts_connection_pool.h"
#include "source/extensions/filters/http/aws_lambda/sts_credentials_provider.h"
#include "source/extensions/filters/http/aws_lambda/sts_fetcher.h"

#include "test/mocks/upstream/mocks.h"

#include "gmock/gmock.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {

class MockStsFetcher : public StsFetcher {
public:
  MOCK_METHOD(void, cancel, ());
  MOCK_METHOD(void, fetch,
              (const envoy::config::core::v3::HttpUri &uri,
               const std::string region,
               const absl::string_view role_arn,
               const absl::string_view web_token,
               StsCredentialsConstSharedPtr creds,
               StsFetcher::Callbacks *callbacks));
};

class MockStsFetcherCallbacks : public StsFetcher::Callbacks {
public:
  MOCK_METHOD(void, onSuccess, (const absl::string_view body));
  MOCK_METHOD(void, onFailure, (CredentialsFailureStatus status));
};

class MockStsContextCallbacks : public StsConnectionPool::Context::Callbacks {
public:
  MOCK_METHOD(
      void, onSuccess,
      (std::shared_ptr<const Envoy::Extensions::Common::Aws::Credentials>));
  MOCK_METHOD(void, onFailure, (CredentialsFailureStatus status));
};

class MockStsConnectionPoolCallbacks : public StsConnectionPool::Callbacks {
public:
  MOCK_METHOD(void, onResult,
              (std::shared_ptr<const StsCredentials> result,
               std::string role_arn, std::list<std::string> &chained_req));
  MOCK_METHOD(void, onFailure, (CredentialsFailureStatus status,
              std::list<std::string>  &chained_requests));
};

class MockStsCredentialsProviderFactory : public StsCredentialsProviderFactory {
public:
  MockStsCredentialsProviderFactory();
  ~MockStsCredentialsProviderFactory() override;

  MOCK_METHOD(StsCredentialsProviderPtr, build,
              (const envoy::config::filter::http::aws_lambda::v2::
                   AWSLambdaConfig_ServiceAccountCredentials &config,
               Event::Dispatcher &dispatcher, std::string_view web_token,
               std::string_view role_arn),
              (const, override));
};

class MockStsConnectionPoolFactory : public StsConnectionPoolFactory {
public:
  MockStsConnectionPoolFactory();
  ~MockStsConnectionPoolFactory() override;

  MOCK_METHOD(StsConnectionPoolPtr, build,
              ( const absl::string_view cache_lookup_arn,
                const absl::string_view role_arn,
               StsConnectionPool::Callbacks *callbacks, StsFetcherPtr fetcher),
              (const, override));
};

class MockStsCredentialsProvider : public StsCredentialsProvider {
public:
  MockStsCredentialsProvider();
  ~MockStsCredentialsProvider() override;

  MOCK_METHOD(StsConnectionPool::Context *, find,
              (const absl::optional<std::string> &role_arn,
              bool disable_role_chaining,
               StsConnectionPool::Context::Callbacks *callbacks));
  MOCK_METHOD(void, setWebToken, (std::string_view web_token));
};

class MockStsConnectionPool : public StsConnectionPool {
public:
  MOCK_METHOD(StsConnectionPool::Context *, add,
              (StsConnectionPool::Context::Callbacks * callback));
  MOCK_METHOD(void, init,
              (const envoy::config::core::v3::HttpUri &uri,
               const std::string region,
               const absl::string_view web_token,
               StsCredentialsConstSharedPtr creds));
  MOCK_METHOD(void, addChained, (std::string role_arn));
  MOCK_METHOD(void, setInFlight, ());
  MOCK_METHOD(bool, requestInFlight, ());
  MOCK_METHOD(void, markFailed, (CredentialsFailureStatus status));
};

class MockStsContext : public StsConnectionPool::Context {
public:
  MockStsContext();

  MOCK_METHOD(void, cancel, ());
  MOCK_METHOD(StsConnectionPool::Context::Callbacks *, callbacks, (), (const));

  testing::NiceMock<MockStsContextCallbacks> callbacks_;
};

// A mock HTTP upstream.
class MockUpstream {
public:
  /**
   * Mock upstream which returns a given response body.
   */
  MockUpstream(Upstream::MockClusterManager &mock_cm, const std::string &status,
               const std::string &response_body);
  /**
   * Mock upstream which returns a given failure.
   */
  MockUpstream(Upstream::MockClusterManager &mock_cm,
               Http::AsyncClient::FailureReason reason);

  /**
   * Mock upstream which returns the given request.
   */
  MockUpstream(Upstream::MockClusterManager &mock_cm,
               Http::MockAsyncClientRequest *request);

private:
  Http::MockAsyncClientRequest request_;
  std::string status_;
  std::string response_body_;
};

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
