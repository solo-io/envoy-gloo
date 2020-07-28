#pragma once

#include <map>
#include <string>

#include "envoy/stats/scope.h"
#include "envoy/stats/stats_macros.h"
#include "common/singleton/const_singleton.h"
#include "envoy/server/transport_socket_config.h"
#include "extensions/filters/http/aws_lambda/stats.h"
#include "extensions/common/aws/credentials_provider.h"

#include "absl/types/optional.h"
#include "api/envoy/config/filter/http/aws_lambda/v2/aws_lambda.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {
  
namespace {
  constexpr char AWS_ROLE_ARN[] = "AWS_ROLE_ARN";
  constexpr char AWS_WEB_IDENTITY_TOKEN_FILE[] = "AWS_WEB_IDENTITY_TOKEN_FILE";
  constexpr char AWS_ROLE_SESSION_NAME[] = "AWS_ROLE_SESSION_NAME";
  constexpr char AWS_STS_REGIONAL_ENDPOINTS[] = "AWS_STS_REGIONAL_ENDPOINTS";
}

class StsConstantValues {
public:
  const std::string RegionalEndpoint{"https://sts.{}.amazonaws.com."};
  const std::string GlobalEndpoint{"https://sts.amazonaws.com."};
};

using StsConstants = ConstSingleton<StsConstantValues>;

class StsCredentialsProvider : public Envoy::Logger::Loggable<Envoy::Logger::Id::aws> {
public:
  StsCredentialsProvider(Api::Api& api, Stats::Scope &scope): api_(api), stats_(generateStats(scope)) {};

  const Envoy::Extensions::Common::Aws::Credentials getCredentials(const std::string* role_arn);

private:
  static AwsLambdaFilterStats generateStats(Stats::Scope &scope);

  absl::optional<std::string> fetchCredentials(
    const std::string& jwt, const std::string& arn);

  
  Api::Api& api_;
  AwsLambdaFilterStats stats_;

  SystemTime last_updated_;
  SystemTime expiration_time_;
  Envoy::Extensions::Common::Aws::Credentials cached_credentials_;

};

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
