#pragma once

#include <map>
#include <string>

#include "envoy/http/filter.h"
#include "envoy/stats/scope.h"
#include "envoy/stats/stats_macros.h"
#include "envoy/upstream/cluster_manager.h"
#include "envoy/server/transport_socket_config.h"
#include "extensions/common/aws/credentials_provider.h"
#include "extensions/filters/http/aws_lambda/stats.h"

#include "absl/types/optional.h"
#include "api/envoy/config/filter/http/aws_lambda/v2/aws_lambda.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {


typedef std::shared_ptr<Envoy::Extensions::Common::Aws::Credentials>
    CredentialsSharedPtr;
typedef std::shared_ptr<const Envoy::Extensions::Common::Aws::Credentials>
    CredentialsConstSharedPtr;

class AWSLambdaConfig {
public:
  virtual CredentialsConstSharedPtr getCredentials() const PURE;
  virtual ~AWSLambdaConfig() = default;
};

class AWSLambdaConfigImpl
    : public AWSLambdaConfig,
      public Envoy::Logger::Loggable<Envoy::Logger::Id::filter> {
public:
  AWSLambdaConfigImpl(
      std::unique_ptr<Envoy::Extensions::Common::Aws::CredentialsProvider>
          &&provider,
      Event::Dispatcher &dispatcher, Envoy::ThreadLocal::SlotAllocator &,
      const std::string &stats_prefix, Stats::Scope &scope,
      const envoy::config::filter::http::aws_lambda::v2::AWSLambdaConfig
          &protoconfig);
  ~AWSLambdaConfigImpl() = default;

  CredentialsConstSharedPtr getCredentials() const override;

private:
  static AwsLambdaFilterStats generateStats(const std::string &prefix,
                                            Stats::Scope &scope);

  void timerCallback();

  std::unique_ptr<Envoy::Extensions::Common::Aws::CredentialsProvider>
      provider_;

  ThreadLocal::SlotPtr tls_slot_;

  Event::TimerPtr timer_;

  AwsLambdaFilterStats stats_;
};

typedef std::shared_ptr<const AWSLambdaConfig> AWSLambdaConfigConstSharedPtr;

class AWSLambdaRouteConfig : public Router::RouteSpecificFilterConfig {
public:
  AWSLambdaRouteConfig(
      const envoy::config::filter::http::aws_lambda::v2::AWSLambdaPerRoute
          &protoconfig);

  const std::string &path() const { return path_; }
  bool async() const { return async_; }
  const absl::optional<std::string> &defaultBody() const {
    return default_body_;
  }

private:
  std::string path_;
  bool async_;
  absl::optional<std::string> default_body_;

  static std::string functionUrlPath(const std::string &name,
                                     const std::string &qualifier);
};

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

class AWSLambdaProtocolExtensionConfig
    : public Upstream::ProtocolOptionsConfig,
      public Envoy::Logger::Loggable<Envoy::Logger::Id::aws> {
public:
  AWSLambdaProtocolExtensionConfig(
      const envoy::config::filter::http::aws_lambda::v2::AWSLambdaProtocolExtension &protoconfig,
      Event::Dispatcher &dispatcher, Envoy::ThreadLocal::SlotAllocator &tlsLocal, Api::Api& api);

  const std::string &host() const { return host_; }
  const std::string &region() const { return region_; }
  const absl::optional<std::string> &accessKey() const { return access_key_; }
  const absl::optional<std::string> &secretKey() const { return secret_key_; }
  const absl::optional<std::string> &sessionToken() const { return session_token_; }

private:
  void timerCallback();
  Envoy::Extensions::Common::Aws::Credentials getCredentials();
  absl::optional<std::string> fetchCredentials(
    const std::string& region, const std::string& jwt, const std::string& arn);

  std::string host_;
  std::string region_;
  absl::optional<std::string> access_key_;
  absl::optional<std::string> secret_key_;
  absl::optional<std::string> session_token_;
  absl::optional<std::string> role_arn_;

  ThreadLocal::SlotPtr tls_slot_;
  Event::TimerPtr timer_;
  Api::Api& api_;

};

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
