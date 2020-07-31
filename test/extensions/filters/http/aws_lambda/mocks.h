#pragma once

#include "envoy/config/core/v3/http_uri.pb.h"

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

class MockStsContext : public StsCredentialsProvider::Context {
public:
  MockStsContext();

  MOCK_METHOD(void, cancel, ());
  MOCK_METHOD(StsFetcher &, fetcher, ());
  MOCK_METHOD(StsCredentialsProvider::Callbacks *, callbacks, (), (const));

  testing::NiceMock<MockStsFetcher> fetcher_;
  testing::NiceMock<MockStsCallbacks> callbacks_;
};

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
