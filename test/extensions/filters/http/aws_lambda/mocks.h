#pragma once

#include "envoy/config/core/v3/http_uri.pb.h"
#include "test/mocks/upstream/mocks.h"

#include "extensions/filters/http/aws_lambda/sts_credentials_provider.h"
#include "extensions/filters/http/aws_lambda/sts_fetcher.h"

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
               const absl::string_view role_arn,
               const absl::string_view web_token,
               StsFetcher::SuccessCallback success,
               StsFetcher::FailureCallback failure));
};

class MockStsCallbacks : public StsCredentialsProvider::Callbacks {
public:
  MOCK_METHOD(
      void, onSuccess,
      (std::shared_ptr<const Envoy::Extensions::Common::Aws::Credentials>));
  MOCK_METHOD(void, onFailure, (CredentialsFailureStatus status));
};

class MockStsCredentialsProviderFactory : public StsCredentialsProviderFactory {
public:
  MockStsCredentialsProviderFactory();
  ~MockStsCredentialsProviderFactory() override;

  MOCK_METHOD(
      StsCredentialsProviderPtr, create,
      (const envoy::config::filter::http::aws_lambda::v2::AWSLambdaConfig_ServiceAccountCredentials& config), (const, override));
};

class MockStsCredentialsProvider : public StsCredentialsProvider {
public:
  MockStsCredentialsProvider();
  ~MockStsCredentialsProvider() override;

  MOCK_METHOD(
      void, find,
      (absl::optional<std::string> role_arn, ContextSharedPtr context));
};

class MockStsContext : public StsCredentialsProvider::Context {
public:
  MockStsContext();

  MOCK_METHOD(void, cancel, ());
  MOCK_METHOD(StsFetcher &, fetcher, ());
  MOCK_METHOD(StsCredentialsProvider::Callbacks *, callbacks, (), (const));

  testing::NiceMock<MockStsFetcher> fetcher_;
  testing::NiceMock<MockStsCallbacks> callbacks_;
};

// A mock HTTP upstream.
class MockUpstream {
public:
  /**
   * Mock upstream which returns a given response body.
   */
  MockUpstream(Upstream::MockClusterManager& mock_cm, const std::string& status,
               const std::string& response_body);
  /**
   * Mock upstream which returns a given failure.
   */
  MockUpstream(Upstream::MockClusterManager& mock_cm, Http::AsyncClient::FailureReason reason);

  /**
   * Mock upstream which returns the given request.
   */
  MockUpstream(Upstream::MockClusterManager& mock_cm, Http::MockAsyncClientRequest* request);

private:
  Http::MockAsyncClientRequest request_;
  std::string status_;
  std::string response_body_;
};

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
