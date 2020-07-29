#pragma once

#include <map>
#include <string>

#include "envoy/http/filter.h"
#include "envoy/stats/scope.h"
#include "envoy/stats/stats_macros.h"
#include "envoy/upstream/cluster_manager.h"
#include "extensions/common/aws/credentials_provider.h"

#include "extensions/filters/http/aws_lambda/sts_credentials_provider.h"

#include "absl/types/optional.h"
#include "api/envoy/config/filter/http/aws_lambda/v2/aws_lambda.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {

/**
 * All stats for the aws filter. @see stats_macros.h
 */
#define ALL_AWS_LAMBDA_FILTER_STATS(COUNTER, GAUGE)                            \
  COUNTER(fetch_failed)                                                        \
  COUNTER(fetch_success)                                                       \
  COUNTER(creds_rotated)                                                       \
  GAUGE(current_state, NeverImport)

/**
 * Wrapper struct for aws filter stats. @see stats_macros.h
 */
struct AwsLambdaFilterStats {
  ALL_AWS_LAMBDA_FILTER_STATS(GENERATE_COUNTER_STRUCT, GENERATE_GAUGE_STRUCT)
};

typedef std::shared_ptr<Envoy::Extensions::Common::Aws::Credentials>
    CredentialsSharedPtr;
typedef std::shared_ptr<const Envoy::Extensions::Common::Aws::Credentials>
    CredentialsConstSharedPtr;

class AWSLambdaConfig {
public:
  virtual ContextSharedPtr getCredentials(std::shared_ptr<const AWSLambdaProtocolExtensionConfig> ext_cfg, StsCredentialsProvider::Callbacks* callbacks) const PURE;
  virtual ~AWSLambdaConfig() = default;
};

class AWSLambdaConfigImpl
    : public AWSLambdaConfig,
      public Envoy::Logger::Loggable<Envoy::Logger::Id::filter> {
public:
  AWSLambdaConfigImpl(
      std::unique_ptr<Envoy::Extensions::Common::Aws::CredentialsProvider> &&provider,
      Upstream::ClusterManager &cluster_manager,
      Event::Dispatcher &dispatcher, Envoy::ThreadLocal::SlotAllocator &,
      const std::string &stats_prefix, Stats::Scope &scope, Api::Api& api,
      const envoy::config::filter::http::aws_lambda::v2::AWSLambdaConfig
          &protoconfig);
  ~AWSLambdaConfigImpl() = default;

  ContextSharedPtr getCredentials(std::shared_ptr<const AWSLambdaProtocolExtensionConfig> ext_cfg, StsCredentialsProvider::Callbacks* callbacks) const override;

private:
  static AwsLambdaFilterStats generateStats(const std::string &prefix,
                                            Stats::Scope &scope);

  void timerCallback();

  ContextFactory context_factory_;

  std::unique_ptr<Envoy::Extensions::Common::Aws::CredentialsProvider>
      provider_;
  
  bool sts_enabled_{};

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

class AWSLambdaProtocolExtensionConfig
    : public Upstream::ProtocolOptionsConfig {
public:
  AWSLambdaProtocolExtensionConfig(
      const envoy::config::filter::http::aws_lambda::v2::
          AWSLambdaProtocolExtension &protoconfig);

  const std::string &host() const { return host_; }
  const std::string &region() const { return region_; }
  const absl::optional<std::string> &accessKey() const { return access_key_; }
  const absl::optional<std::string> &secretKey() const { return secret_key_; }
  const absl::optional<std::string> &sessionToken() const { return session_token_; }
  const absl::optional<std::string> &roleArn() const { return role_arn_; }

private:
  std::string host_;
  std::string region_;
  absl::optional<std::string> access_key_;
  absl::optional<std::string> secret_key_;
  absl::optional<std::string> session_token_;
  absl::optional<std::string> role_arn_;
};

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
