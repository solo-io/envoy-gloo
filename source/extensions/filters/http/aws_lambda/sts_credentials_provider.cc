#include "extensions/filters/http/aws_lambda/sts_credentials_provider.h"

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
  
/*
  * AssumeRoleWithIdentity returns a set of temporary credentials with a minimum lifespan of 15 minutes.
  * https://docs.aws.amazon.com/STS/latest/APIReference/API_AssumeRoleWithWebIdentity.html
  * 
  * In order to ensure that credentials never expire, we default to 2/3.
  * 
  * This in combination with the very generous grace period which makes sure the tokens are 
  * refreshed if they have < 5 minutes left on their lifetime. Whether that lifetime is 
  * our prescribed, or from the response itself.
*/
constexpr std::chrono::milliseconds REFRESH_STS_CREDS =
    std::chrono::minutes(10);

constexpr std::chrono::minutes REFRESH_GRACE_PERIOD{5};

constexpr char EXPIRATION_FORMAT[] = "%E4Y%m%dT%H%M%S%z";
  
class ThreadLocalStsCache : public Envoy::ThreadLocal::ThreadLocalObject {
public:
  ThreadLocalStsCache(absl::string_view web_token): web_token_(web_token) {};

  const absl::string_view webToken() const {return web_token_;};

  void setWebToken(absl::string_view web_token)  { web_token_ = web_token; };
  
  std::unordered_map<std::string, StsCredentialsConstSharedPtr> credentialsCache() {return credentials_cache_;};

private:
  // web_token set by AWS, will be auto-updated by StsCredentialsProvider
  // TODO: udpate this file, inotify or timer
  absl::string_view web_token_;
  // Credentials storage map, keyed by arn
  std::unordered_map<std::string, StsCredentialsConstSharedPtr> credentials_cache_;
};

using ThreadLocalStsCacheSharedPtr = std::shared_ptr<ThreadLocalStsCache>;


class ContextImpl : public StsCredentialsProvider::Context {
public:
  ContextImpl(Upstream::ClusterManager& cm, Api::Api& api, StsCredentialsProvider::Callbacks* callback)
      : fetcher_(StsFetcher::create(cm, api)), callbacks_(callback) {}

  StsCredentialsProvider::Callbacks* callbacks() const override { return callbacks_; }
  StsFetcherPtr& fetcher() override { return fetcher_; }

  void cancel() override {
    fetcher_->cancel();
  }

private:
  StsFetcherPtr fetcher_;
  StsCredentialsProvider::Callbacks* callbacks_;
};


class StsCredentialsProviderImpl: public StsCredentialsProvider, Logger::Loggable<Logger::Id::aws> {
public:
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
    tls_slot_->set([&web_token](Event::Dispatcher &) {
      ThreadLocalStsCache cache(web_token);
      return std::make_shared<ThreadLocalStsCache>(std::move(cache));
    });

    // Add file watcher for token file
    file_watcher_->addWatch(token_file_, Filesystem::Watcher::Events::Modified, [this](uint32_t) {
      auto tls_cache = tls_slot_->getTyped<ThreadLocalStsCacheSharedPtr>();
      const auto web_token = api_.fileSystem().fileReadToEnd(token_file_);
      // TODO: stats here
      tls_cache->setWebToken(web_token);
    });

    // Initialize regex strings, should never fail
    access_key_regex_ = Regex::Utility::parseStdRegex("<AccessKeyId>.*?<\\/AccessKeyId>");
    secret_key_regex_ = Regex::Utility::parseStdRegex("<SecretAccessKey>.*?<\\/SecretAccessKey>");
    session_token_regex_ = Regex::Utility::parseStdRegex("<SessionToken>.*?<\\/SessionToken>");
    expiration_regex_ = Regex::Utility::parseStdRegex("<Expiration>.*?<\\/Expiration>");

  }

  void find(absl::optional<std::string> role_arn_arg, ContextSharedPtr context) override {
    auto& ctximpl = static_cast<Context&>(*context);

    std::string role_arn = default_role_arn_;
    // If role_arn_arg is present, use that, otherwise use env
    if (role_arn_arg.has_value()) {
      role_arn = std::string(role_arn_arg.value());
    }

    ASSERT(!role_arn.empty());

    auto tls_cache = tls_slot_->getTyped<ThreadLocalStsCacheSharedPtr>();

    const auto it = tls_cache->credentialsCache().find(role_arn);
    if (it != tls_cache->credentialsCache().end()) {
      // thing  exists
      const auto now = api_.timeSource().systemTime();
      // If the expiration time is more than a minute away, return it immediately
      if (it->second->expirationTime() - now > REFRESH_GRACE_PERIOD) {
        ctximpl.callbacks()->onSuccess(it->second);
        return;        
      }
      // token is considered expired, fallthrough to create a new one
    }
  
    ctximpl.fetcher()->fetch(
      uri_, 
      role_arn, 
      tls_cache->webToken(), 
      [this, &ctximpl, role_arn, &tls_cache](const std::string* body) {
        ASSERT(body != nullptr);

        //TODO: (yuval): create utility function for this regex search
        std::smatch matched_access_key;
        std::regex_search(*body, matched_access_key, access_key_regex_);
        if (!(matched_access_key.size() > 1)) {
          ENVOY_LOG(trace, "response body did not contain access_key");
          ctximpl.callbacks()->onFailure(CredentialsFailureStatus::InvalidSts);
        }

        std::smatch matched_secret_key;
        std::regex_search(*body, matched_secret_key, secret_key_regex_);
        if (!(matched_secret_key.size() > 1)) {
          ENVOY_LOG(trace, "response body did not contain secret_key");
          ctximpl.callbacks()->onFailure(CredentialsFailureStatus::InvalidSts);
        }
        
        std::smatch matched_session_token;
        std::regex_search(*body, matched_session_token, session_token_regex_);
        if (!(matched_session_token.size() > 1)) {
          ENVOY_LOG(trace, "response body did not contain session_token");
          ctximpl.callbacks()->onFailure(CredentialsFailureStatus::InvalidSts);
        }
        
        std::smatch matched_expiration;
        std::regex_search(*body, matched_expiration, expiration_regex_);
        if (!(matched_expiration.size() > 1)) {
          ENVOY_LOG(trace, "response body did not contain expiration");
          ctximpl.callbacks()->onFailure(CredentialsFailureStatus::InvalidSts);
        }

        SystemTime expiration_time;
        absl::Time absl_expiration_time;
        if (absl::ParseTime(EXPIRATION_FORMAT, matched_expiration[1].str(), &absl_expiration_time, nullptr)) {
          ENVOY_LOG(trace, "Determined expiration time from STS credentials result");
          expiration_time = absl::ToChronoTime(absl_expiration_time);
        } else {
          expiration_time = api_.timeSource().systemTime() + REFRESH_STS_CREDS;
          ENVOY_LOG(trace, "Unable to determine expiration time from STS credentials result, using default");
        }

        StsCredentialsConstSharedPtr result = std::make_shared<const StsCredentials>(matched_access_key[1].str(), matched_secret_key[1].str(), matched_session_token[1].str(), expiration_time);
        
        // Success callback, save back to cache
        tls_cache->credentialsCache().emplace(role_arn, result);
        ctximpl.callbacks()->onSuccess(result);
      },
      [this, &ctximpl](CredentialsFailureStatus reason) {
        // unsuccessful, send back empty creds?
        ctximpl.callbacks()->onFailure(reason);
      }
    );
  };


private:

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

ContextSharedPtr ContextFactory::create(StsCredentialsProvider::Callbacks* callbacks) const {
  return std::make_shared<ContextImpl>(cm_, api_, callbacks);
};

StsCredentialsProviderPtr StsCredentialsProvider::create(
  const envoy::config::filter::http::aws_lambda::v2::AWSLambdaConfig_ServiceAccountCredentials& config,
   Api::Api& api, ThreadLocal::SlotAllocator& tls, Event::Dispatcher &dispatcher) {

  return std::make_unique<StsCredentialsProviderImpl>(config, api, tls, dispatcher);
};

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
