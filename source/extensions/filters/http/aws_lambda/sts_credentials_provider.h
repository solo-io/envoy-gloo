#pragma once

#include "envoy/api/api.h"
#include "envoy/common/pure.h"
#include "envoy/common/time.h"
#include "common/common/regex.h"
#include "extensions/common/aws/credentials_provider.h"
#include "extensions/filters/http/aws_lambda/sts_fetcher.h"

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

class StsCredentialsProvider;
using StsCredentialsProviderPtr = std::shared_ptr<StsCredentialsProvider>;

class StsCredentialsProvider {
public:
  virtual ~StsCredentialsProvider() = default;

  class Callbacks {
  public:
    virtual ~Callbacks() = default;

    /**
     * Called on successful request
     *
     * @param credential the credentials
     */
    virtual void onSuccess(std::shared_ptr<const Envoy::Extensions::Common::Aws::Credentials>) PURE;

    /**
     * Called on completion of request.
     *
     * @param status the status of the request.
     */
    virtual void onFailure(CredentialsFailureStatus status) PURE;
  };

  // Context object to hold data needed for verifier.
  class Context {
  public:
    virtual ~Context() = default;

    /**
     * Returns the request callback wrapped in this context.
     *
     * @returns the request callback.
     */
    virtual Callbacks* callbacks() const PURE;


    /**
     * Returns the request callback wrapped in this context.
     *
     * @returns the fetcher.
     */
    virtual StsFetcher& fetcher() PURE;

    /**
     * Cancel any pending requests for this context.
     */
    virtual void cancel() PURE;
  };

  using ContextSharedPtr = std::shared_ptr<Context>;


  // Lookup credentials cache map. The cache only stores Jwks specified in the config.
  virtual void find(absl::optional<std::string> role_arn, ContextSharedPtr context) PURE;

};

class ThreadLocalStsCache : public Envoy::ThreadLocal::ThreadLocalObject {
public:
  ThreadLocalStsCache(absl::string_view web_token): web_token_(web_token) {};

  const absl::string_view webToken() const {return web_token_;};

  void setWebToken(absl::string_view web_token)  { web_token_ = std::string(web_token); };
  
  std::unordered_map<std::string, StsCredentialsConstSharedPtr> credentialsCache() {return credentials_cache_;};

private:
  // web_token set by AWS, will be auto-updated by StsCredentialsProvider
  // TODO: udpate this file, inotify or timer
  std::string web_token_;
  // Credentials storage map, keyed by arn
  std::unordered_map<std::string, StsCredentialsConstSharedPtr> credentials_cache_;
};

class StsCredentialsProviderImpl: public StsCredentialsProvider,
                                  public Logger::Loggable<Logger::Id::aws>,
                                  public std::enable_shared_from_this<StsCredentialsProviderImpl> {

public:

  void find(absl::optional<std::string> role_arn_arg, ContextSharedPtr context) override;

    // Factory function to create an instance.
  static StsCredentialsProviderPtr
  create(const envoy::config::filter::http::aws_lambda::v2::AWSLambdaConfig_ServiceAccountCredentials& config, 
      Api::Api& api, ThreadLocal::SlotAllocator& tls, Event::Dispatcher &dispatcher) {

    std::shared_ptr<StsCredentialsProviderImpl> ptr(new StsCredentialsProviderImpl(config, api, tls, dispatcher));
    ptr->init();
    return ptr;
  };

private:

  StsCredentialsProviderImpl(
    const envoy::config::filter::http::aws_lambda::v2::AWSLambdaConfig_ServiceAccountCredentials& config,
    Api::Api& api, ThreadLocal::SlotAllocator& tls, Event::Dispatcher &dispatcher) : api_(api), config_(config), 
    default_role_arn_(absl::NullSafeStringView(std::getenv(AWS_ROLE_ARN))),
    token_file_(absl::NullSafeStringView(std::getenv(AWS_WEB_IDENTITY_TOKEN_FILE))), tls_slot_(tls.allocateSlot()),
    file_watcher_(dispatcher.createFilesystemWatcher()) {

    uri_.set_cluster(config_.cluster());
    uri_.set_uri(config_.uri());
    // TODO: Figure out how to get this to compile, timeout is not all that important right now
    // uri_.set_allocated_timeout(config_.mutable_timeout())

    // AWS_WEB_IDENTITY_TOKEN_FILE and AWS_ROLE_ARN must be set for STS credentials to be enabled
    if (token_file_ == "") {
      throw EnvoyException(fmt::format("Env var {} must be present, and set", AWS_WEB_IDENTITY_TOKEN_FILE));
    }
    if (default_role_arn_ == "") {
      throw EnvoyException(fmt::format("Env var {} must be present, and set", AWS_ROLE_ARN));
    }
    // File must exist on system
    if (!api_.fileSystem().fileExists(token_file_)) {
      throw EnvoyException(fmt::format("Web token file {} does not exist", token_file_));
    }

    const auto web_token = api_.fileSystem().fileReadToEnd(token_file_);
    // File should not be empty
    if (web_token == "") {
      throw EnvoyException(fmt::format("Web token file {} exists but is empty", token_file_));
    }

    // create a thread local cache for the provider
    tls_slot_->set([web_token](Event::Dispatcher &) {
      return std::make_shared<ThreadLocalStsCache>(web_token);
    });

    // Initialize regex strings, should never fail
    access_key_regex_ = Regex::Utility::parseStdRegex("<AccessKeyId>(.*?)</AccessKeyId>");
    secret_key_regex_ = Regex::Utility::parseStdRegex("<SecretAccessKey>(.*?)</SecretAccessKey>");
    session_token_regex_ = Regex::Utility::parseStdRegex("<SessionToken>(.*?)</SessionToken>");
    expiration_regex_ = Regex::Utility::parseStdRegex("<Expiration>(.*?)</Expiration>");
  };
  
  void init();

  Api::Api& api_;
  const envoy::config::filter::http::aws_lambda::v2::AWSLambdaConfig_ServiceAccountCredentials& config_;

  std::string default_role_arn_;
  std::string token_file_;
  envoy::config::core::v3::HttpUri uri_;
  ThreadLocal::SlotPtr tls_slot_;

  std::regex access_key_regex_;
  std::regex secret_key_regex_;
  std::regex session_token_regex_;
  std::regex expiration_regex_;

  Envoy::Filesystem::WatcherPtr file_watcher_;
};

using ContextSharedPtr = std::shared_ptr<StsCredentialsProvider::Context>;

class ContextFactory {
public:
  ContextFactory(Upstream::ClusterManager& cm, Api::Api& api) : cm_(cm), api_(api) {};

  virtual ~ContextFactory() = default;

  virtual ContextSharedPtr create(StsCredentialsProvider::Callbacks* callbacks) const;

private:
  Upstream::ClusterManager& cm_;
  Api::Api& api_;
};

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
