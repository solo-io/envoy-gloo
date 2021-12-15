#pragma once

#include <map>
#include <string>

#include "envoy/http/filter.h"
#include "envoy/stats/scope.h"
#include "envoy/stats/stats_macros.h"
#include "envoy/upstream/cluster_manager.h"

#include "source/extensions/common/aws/credentials_provider.h"
#include "source/extensions/filters/http/aws_lambda/sts_credentials_provider.h"

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

using CredentialsSharedPtr =
    std::shared_ptr<Envoy::Extensions::Common::Aws::Credentials>;
using CredentialsConstSharedPtr =
    std::shared_ptr<const Envoy::Extensions::Common::Aws::Credentials>;

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
  const absl::optional<std::string> &sessionToken() const {
    return session_token_;
  }
  const absl::optional<std::string> &roleArn() const { return role_arn_; }

private:
  std::string host_;
  std::string region_;
  absl::optional<std::string> access_key_;
  absl::optional<std::string> secret_key_;
  absl::optional<std::string> session_token_;
  absl::optional<std::string> role_arn_;
};

using SharedAWSLambdaProtocolExtensionConfig =
    std::shared_ptr<const AWSLambdaProtocolExtensionConfig>;

class AWSLambdaConfig {
public:
  virtual StsConnectionPool::Context *
  getCredentials(SharedAWSLambdaProtocolExtensionConfig ext_cfg,
                 StsConnectionPool::Context::Callbacks *callbacks) const PURE;
  virtual bool propagateOriginalRouting() const PURE;
  virtual ~AWSLambdaConfig() = default;
};

class AWSLambdaConfigImpl
    : public AWSLambdaConfig,
      public Envoy::Logger::Loggable<Envoy::Logger::Id::filter>,
      public std::enable_shared_from_this<AWSLambdaConfigImpl> {
public:
  ~AWSLambdaConfigImpl() = default;

  static std::shared_ptr<AWSLambdaConfigImpl>
  create(std::unique_ptr<Envoy::Extensions::Common::Aws::CredentialsProvider>
             &&provider,
         std::unique_ptr<StsCredentialsProviderFactory> &&sts_factory,
         Event::Dispatcher &dispatcher, Api::Api &api,
         Envoy::ThreadLocal::SlotAllocator &tls,
         const std::string &stats_prefix, Stats::Scope &scope,
         const envoy::config::filter::http::aws_lambda::v2::AWSLambdaConfig
             &protoconfig);

  StsConnectionPool::Context *getCredentials(
      SharedAWSLambdaProtocolExtensionConfig ext_cfg,
      StsConnectionPool::Context::Callbacks *callbacks) const override;

    bool propagateOriginalRouting() const override{
      return propagate_original_routing_;
    }

private:
  AWSLambdaConfigImpl(
      std::unique_ptr<Envoy::Extensions::Common::Aws::CredentialsProvider>
          &&provider,
      std::unique_ptr<StsCredentialsProviderFactory> &&sts_factory,
      Event::Dispatcher &dispatcher, Api::Api &api,
      Envoy::ThreadLocal::SlotAllocator &tls, const std::string &stats_prefix,
      Stats::Scope &scope,
      const envoy::config::filter::http::aws_lambda::v2::AWSLambdaConfig
          &protoconfig);


  struct ThreadLocalCredentials : public Envoy::ThreadLocal::ThreadLocalObject {
    ThreadLocalCredentials(CredentialsConstSharedPtr credentials)
        : credentials_(credentials) {}
    ThreadLocalCredentials(StsCredentialsProviderPtr credentials)
        : sts_credentials_(std::move(credentials)) {}
    CredentialsConstSharedPtr credentials_;
    StsCredentialsProviderPtr sts_credentials_;
  };

  CredentialsConstSharedPtr getProviderCredentials() const;

  static AwsLambdaFilterStats generateStats(const std::string &prefix,
                                            Stats::Scope &scope);

  void timerCallback();

  void init();

  void loadSTSData();

  AwsLambdaFilterStats stats_;

  Api::Api &api_;

  Envoy::Filesystem::WatcherPtr file_watcher_;

  std::unique_ptr<Envoy::Extensions::Common::Aws::CredentialsProvider>
      provider_;

  bool sts_enabled_ = false;
  std::string token_file_;
  std::string web_token_;
  std::string role_arn_;

  ThreadLocal::TypedSlot<ThreadLocalCredentials> tls_;

  Event::TimerPtr timer_;

  std::unique_ptr<StsCredentialsProviderFactory> sts_factory_;

  bool propagate_original_routing_;
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
  bool unwrapAsAlb() const { return unwrap_as_alb_; }

private:
  std::string path_;
  bool async_;
  bool unwrap_as_alb_;
  absl::optional<std::string> default_body_;

  static std::string functionUrlPath(const std::string &name,
                                     const std::string &qualifier);
};

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
