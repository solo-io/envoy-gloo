#include "extensions/filters/http/aws_lambda/config.h"

#include "curl/curl.h"
#include "common/common/regex.h"
#include "envoy/thread_local/thread_local.h"
#include "extensions/filters/http/aws_lambda/stats.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {

namespace CommonAws = Envoy::Extensions::Common::Aws;

namespace {

// Current AWS implementation will only refresh creds if *more* than an hour has
// passed. In the AWS impl these creds are fetched on the main thread. As we use
// the creds in the filter, we cache the creds in a TLS slot, so they are
// available to the filters in the worker threads. As we have our own cache, we
// have to implement our own refresh loop. To avoid being to tied down the
// timing of the AWS implementation, we'll have a refresh interval of a few
// minutes. refresh itself will only happen when deemed needed by the AWS code.
//
// According to the AWS docs
// (https://docs.aws.amazon.com/IAM/latest/UserGuide/id_roles_use_switch-role-ec2.html)
//
// >>> it should get a refreshed set of credentials every hour, or at least 15
// >>> minutes before the current set expires.
//
// Refreshing every 14 minutes should guarantee us fresh credentials.
constexpr std::chrono::milliseconds REFRESH_AWS_CREDS =
    std::chrono::minutes(14);

struct ThreadLocalState : public Envoy::ThreadLocal::ThreadLocalObject {
  ThreadLocalState(CredentialsConstSharedPtr credentials)
      : credentials_(credentials) {}
  CredentialsConstSharedPtr credentials_;
};

} // namespace

AWSLambdaConfigImpl::AWSLambdaConfigImpl(
    std::unique_ptr<Extensions::Common::Aws::CredentialsProvider> &&provider,
    Event::Dispatcher &dispatcher, Envoy::ThreadLocal::SlotAllocator &tls,
    const std::string &stats_prefix, Stats::Scope &scope,
    const envoy::config::filter::http::aws_lambda::v2::AWSLambdaConfig
        &protoconfig)
    : stats_(generateStats(stats_prefix, scope)) {
  bool use_default_credentials = false;

  if (protoconfig.has_use_default_credentials()) {
    use_default_credentials = protoconfig.use_default_credentials().value();
  }

  if (use_default_credentials) {
    provider_ = std::move(provider);

    tls_slot_ = tls.allocateSlot();
    auto empty_creds = std::make_shared<const CommonAws::Credentials>();
    tls_slot_->set([empty_creds](Event::Dispatcher &) {
      return std::make_shared<ThreadLocalState>(empty_creds);
    });

    timer_ = dispatcher.createTimer([this] { timerCallback(); });
    // call the time callback to fetch credentials now.
    // this will also re-trigger the timer.
    timerCallback();
  }
}

CredentialsConstSharedPtr AWSLambdaConfigImpl::getCredentials() const {
  if (!provider_) {
    return {};
  }

  // tls_slot_ != nil IFF provider_ != nil
  return tls_slot_->getTyped<ThreadLocalState>().credentials_;
}

void AWSLambdaConfigImpl::timerCallback() {
  // get new credentials.
  auto new_creds = provider_->getCredentials();
  if (new_creds == CommonAws::Credentials()) {
    stats_.fetch_failed_.inc();
    stats_.current_state_.set(0);
    ENVOY_LOG(warn, "can't get AWS credentials - credentials will not be "
                    "refreshed and request to AWS may fail");
  } else {
    stats_.fetch_success_.inc();
    stats_.current_state_.set(1);
    auto currentCreds = getCredentials();
    if (currentCreds == nullptr || !((*currentCreds) == new_creds)) {
      stats_.creds_rotated_.inc();
      ENVOY_LOG(debug, "refreshing AWS credentials");
      auto shared_new_creds =
          std::make_shared<const CommonAws::Credentials>(new_creds);
      tls_slot_->set([shared_new_creds](Event::Dispatcher &) {
        return std::make_shared<ThreadLocalState>(shared_new_creds);
      });
    }
  }

  if (timer_ != nullptr) {
    // re-enable refersh timer
    timer_->enableTimer(REFRESH_AWS_CREDS);
  }
}

AwsLambdaFilterStats
AWSLambdaConfigImpl::generateStats(const std::string &prefix,
                                   Stats::Scope &scope) {
  const std::string final_prefix = prefix + "aws_lambda.";
  return {ALL_AWS_LAMBDA_FILTER_STATS(POOL_COUNTER_PREFIX(scope, final_prefix),
                                      POOL_GAUGE_PREFIX(scope, final_prefix))};
}

AWSLambdaRouteConfig::AWSLambdaRouteConfig(
    const envoy::config::filter::http::aws_lambda::v2::AWSLambdaPerRoute
        &protoconfig)
    : path_(functionUrlPath(protoconfig.name(), protoconfig.qualifier())),
      async_(protoconfig.async()) {

  if (protoconfig.has_empty_body_override()) {
    default_body_ = protoconfig.empty_body_override().value();
  }
}

std::string
AWSLambdaRouteConfig::functionUrlPath(const std::string &name,
                                      const std::string &qualifier) {

  std::stringstream val;
  val << "/2015-03-31/functions/" << name << "/invocations";
  if (!qualifier.empty()) {
    val << "?Qualifier=" << qualifier;
  }
  return val.str();
}

/*
  Based on AWS STS docs the minimum lifetime for temporary credentials returned from
  AssumeRoleWithWebIdentity is 15 minutes.
  https://docs.aws.amazon.com/STS/latest/APIReference/API_AssumeRoleWithWebIdentity.html

  Therefore attempting to refresh every 3 minutes should guarantee that every time 
  credentials are grabbed by the filter, they will be up to date.
*/
constexpr std::chrono::milliseconds REFRESH_STS_CREDS =
    std::chrono::minutes(3);

AWSLambdaProtocolExtensionConfig::AWSLambdaProtocolExtensionConfig(
    const envoy::config::filter::http::aws_lambda::v2::AWSLambdaProtocolExtension &protoconfig,
    Event::Dispatcher &dispatcher, Envoy::ThreadLocal::SlotAllocator &tls, Api::Api& api)
    : host_(protoconfig.host()), region_(protoconfig.region()), api_(api) {
  if (!protoconfig.access_key().empty()) {
    access_key_ = protoconfig.access_key();
  }
  if (!protoconfig.secret_key().empty()) {
    secret_key_ = protoconfig.secret_key();
  }
  if (!protoconfig.session_token().empty()) {
    session_token_ = protoconfig.session_token();
  }
  if (!protoconfig.role_arn().empty()) {
    role_arn_ = protoconfig.role_arn();
  }
  tls_slot_ = tls.allocateSlot();
  auto empty_creds = std::make_shared<const CommonAws::Credentials>();
  tls_slot_->set([empty_creds](Event::Dispatcher &) {
    return std::make_shared<ThreadLocalState>(empty_creds);
  });

  timer_ = dispatcher.createTimer([this] { timerCallback(); });
  // call the time callback to fetch credentials now.
  // this will also re-trigger the timer.
  timerCallback();

}

void AWSLambdaProtocolExtensionConfig::timerCallback() {
  auto new_creds = getCredentials();
  auto current_creds = tls_slot_->getTyped<ThreadLocalState>().credentials_;
  if (current_creds == nullptr || !((*current_creds) == new_creds)) {
    ENVOY_LOG(debug, "refreshing AWS credentials");
    auto shared_new_creds =
        std::make_shared<const CommonAws::Credentials>(new_creds);
    tls_slot_->set([shared_new_creds](Event::Dispatcher &) {
      return std::make_shared<ThreadLocalState>(shared_new_creds);
    });
  }

  if (timer_ != nullptr) {
    // re-enable refersh timer
    timer_->enableTimer(REFRESH_STS_CREDS);
  }
}

CommonAws::Credentials AWSLambdaProtocolExtensionConfig::getCredentials() {
  const auto token_file  = absl::NullSafeStringView(std::getenv(AWS_WEB_IDENTITY_TOKEN_FILE));
  ASSERT(!token_file.empty());
  // File must exist on system
  ASSERT(api_.fileSystem().fileExists(std::string(token_file)));

  const auto web_token = api_.fileSystem().fileReadToEnd(std::string(token_file));

  // Set ARN to env var
  std::string role_arn{}; 
  // If role_arn_ is present in protocol options, use that
  if (role_arn_.has_value()) {
    role_arn = std::string(role_arn_.value());
  }  else {
    role_arn = absl::NullSafeStringView(std::getenv(AWS_ROLE_ARN));
  }
  // role_arn cannot be empty
  // ASSERT(role_arn != nullptr);
  
  const auto token_body = fetchCredentials(region_, web_token, role_arn);

  if (token_body.has_value()) {
    ENVOY_LOG(error, "Could not fetch credentials via STS");
    return CommonAws::Credentials();
  }

  const auto access_key_regex_ = Regex::Utility::parseStdRegex("<AccessKeyId>.*?<\\/AccessKeyId>");
  const auto secret_key_regex_ = Regex::Utility::parseStdRegex("<SecretAccessKey>.*?<\\/SecretAccessKey>");
  const auto session_token_regex_ = Regex::Utility::parseStdRegex("<SessionToken>.*?<\\/SessionToken>");
  const auto expiration_regex_ = Regex::Utility::parseStdRegex("<Expiration>.*?<\\/Expiration>");
  
  std::smatch matched;
  std::regex_search(token_body.value(), matched, access_key_regex_);
  if (matched.size() < 1) {
    return CommonAws::Credentials();
  }

  return CommonAws::Credentials();
}

static size_t curlCallback(char* ptr, size_t, size_t nmemb, void* data) {
  auto buf = static_cast<std::string*>(data);
  buf->append(ptr, nmemb);
  return nmemb;
}

absl::optional<std::string> AWSLambdaProtocolExtensionConfig::fetchCredentials(
    const std::string& , const std::string& jwt, const std::string& arn)  {
        static const size_t MAX_RETRIES = 4;
  static const std::chrono::milliseconds RETRY_DELAY{1000};
  static const std::chrono::seconds TIMEOUT{5};

  CURL* const curl = curl_easy_init();
  if (!curl) {
    return absl::nullopt;
  };

  curl_easy_setopt(curl, CURLOPT_URL, StsConstants::get().GlobalEndpoint.c_str());
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, TIMEOUT.count());
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

  const std::string post_params = fmt::format("Action=AssumeRoleWithWebIdentity&RoleArn={}&RoleSessionName={}&WebIdentityToken={}", arn, "session_name", jwt);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_params);

  std::string buffer;
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlCallback);

  for (size_t retry = 0; retry < MAX_RETRIES; retry++) {
    const CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
      break;
    }
    // ENVOY_LOG_MISC(debug, "Could not AssumeRoleWithWebIdentity: {}", curl_easy_strerror(res));
    buffer.clear();
    std::this_thread::sleep_for(RETRY_DELAY);
  }

  curl_easy_cleanup(curl);

  return buffer.empty() ? absl::nullopt : absl::optional<std::string>(buffer);
}

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
