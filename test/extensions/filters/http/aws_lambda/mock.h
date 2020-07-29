#pragma once

#include "extensions/common/aws/credentials_provider.h"
#include "envoy/config/core/v3/http_uri.pb.h"
#include "extensions/filters/http/common/jwks_fetcher.h"
#include "test/mocks/server/mocks.h"
#include "gmock/gmock.h"

using Envoy::Extensions::Common;

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {

class MockStsFetcher : public StsFetcher {
public:
  MOCK_METHOD(void, cancel, ());
  MOCK_METHOD(void, fetch,
              (const envoy::config::core::v3::HttpUri& uri,
                      const std::string& role_arn,
                      const std::string& web_token,
                      StsReceiver& receiver));
};

class MockStsReceiver : public StsFetcher::StsReceiver {
public:
  /* GoogleMock does handle r-value references hence the below construction.
   * Expectations and assertions should be made on onStsSuccessImpl in place
   * of onStsSuccess.
   */
  void onStsSuccess(Aws::Credentials&& credentials, const SystemTime& system_time) override {
    ASSERT(credentials);
    onStsSuccessImpl(*jwks.get());
  }
  MOCK_METHOD(void, onStsSuccessImpl, (const Aws::Credentials& credentials, const SystemTime& )expiration);
  MOCK_METHOD(void, onStsError, (StsFetcher::StsReceiver::Failure reason));
};

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
