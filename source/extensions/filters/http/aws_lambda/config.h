#pragma once

#include <map>
#include <string>

#include "envoy/http/filter.h"
#include "envoy/stats/scope.h"
#include "envoy/stats/stats_macros.h"
#include "envoy/upstream/cluster_manager.h"

#include "source/extensions/common/aws/credentials_provider.h"
#include "source/extensions/filters/http/aws_lambda/sts_credentials_provider.h"
#include "source/extensions/filters/http/transformation/transformer.h"

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
  COUNTER(webtoken_rotated)                                                    \
  COUNTER(webtoken_failure)                                                    \
  GAUGE(webtoken_state, NeverImport)                                           \
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
  const bool &disableRoleChaining() const { return disable_role_chaining_; }

private:
  std::string host_;
  std::string region_;
  absl::optional<std::string> access_key_;
  absl::optional<std::string> secret_key_;
  absl::optional<std::string> session_token_;
  absl::optional<std::string> role_arn_;
  bool disable_role_chaining_;
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
      public Envoy::Logger::Loggable<Envoy::Logger::Id::filter> {
public:
  ~AWSLambdaConfigImpl();
  // Non-copyable, as sts_refesher holds a raw pointer to this.
  AWSLambdaConfigImpl(const AWSLambdaConfigImpl&) = delete;
  AWSLambdaConfigImpl& operator=(const AWSLambdaConfigImpl&) = delete;
  // Non-movable, same reason as non-copyable.
  AWSLambdaConfigImpl(AWSLambdaConfigImpl&&) = delete;
  AWSLambdaConfigImpl& operator=(AWSLambdaConfigImpl&&) = delete;

  AWSLambdaConfigImpl(Extensions::Common::Aws::CredentialsProviderSharedPtr provider,
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

  class AWSLambdaStsRefresher :
        public std::enable_shared_from_this<AWSLambdaStsRefresher> {
  public:
    ~AWSLambdaStsRefresher() = default;

    AWSLambdaStsRefresher(AWSLambdaConfigImpl* parent, Event::Dispatcher &dispatcher);

    void cancel();
    void init(Event::Dispatcher &dispatcher);

private:
    AWSLambdaConfigImpl* parent_;

    Envoy::Filesystem::WatcherPtr file_watcher_;
    Event::TimerPtr timer_;
  };
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

  void init(Event::Dispatcher &dispatcher);

  void loadSTSData();

  AwsLambdaFilterStats stats_;

  Api::Api &api_;

  Extensions::Common::Aws::CredentialsProviderSharedPtr provider_;

  ThreadLocal::TypedSlot<ThreadLocalCredentials> tls_;
  std::string token_file_;
  std::string web_token_;
  std::string role_arn_;

  std::shared_ptr<AWSLambdaStsRefresher> sts_refresher_;

  Event::TimerPtr timer_;

  std::unique_ptr<StsCredentialsProviderFactory> sts_factory_;
  std::chrono::milliseconds credential_refresh_delay_;

  bool propagate_original_routing_;
};

typedef std::shared_ptr<const AWSLambdaConfig> AWSLambdaConfigConstSharedPtr;

class AWSLambdaRouteConfig : public Router::RouteSpecificFilterConfig {
public:
  AWSLambdaRouteConfig(
      const envoy::config::filter::http::aws_lambda::v2::AWSLambdaPerRoute &protoconfig,
      Server::Configuration::ServerFactoryContext &context
    );

  const std::string &path() const { return path_; }
  bool async() const { return async_; }
  const absl::optional<std::string> &defaultBody() const {
    return default_body_;
  }
  bool unwrapAsAlb() const { return unwrap_as_alb_; }
  Transformation::TransformerConstSharedPtr transformerConfig() const { return transformer_config_; }
  Transformation::TransformerConstSharedPtr requestTransformerConfig() const { return request_transformer_config_; }
  bool hasTransformerConfig() const { return has_transformer_config_; }
  bool hasRequestTransformerConfig() const { return request_transformer_config_ != nullptr; }
private:
  std::string path_;
  bool async_;
  bool unwrap_as_alb_;
  Transformation::TransformerConstSharedPtr transformer_config_;
  bool has_transformer_config_;
  Transformation::TransformerConstSharedPtr request_transformer_config_;
  absl::optional<std::string> default_body_;

  static std::string functionUrlPath(const std::string &name,
                                     const std::string &qualifier);
};

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
