#include "extensions/filters/http/aws_lambda/sts_credentials_provider.h"

#include "envoy/api/api.h"
#include "envoy/common/pure.h"
#include "envoy/common/time.h"

#include "common/common/linked_object.h"

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

constexpr std::chrono::minutes REFRESH_GRACE_PERIOD{5};
} // namespace

StsCredentialsProviderImpl::StsCredentialsProviderImpl(
    const envoy::config::filter::http::aws_lambda::v2::
        AWSLambdaConfig_ServiceAccountCredentials &config,
    Api::Api &api, Event::Dispatcher &dispatcher)
    : api_(api), dispatcher_(dispatcher), config_(config),
      default_role_arn_(absl::NullSafeStringView(std::getenv(AWS_ROLE_ARN))),
      token_file_(
          absl::NullSafeStringView(std::getenv(AWS_WEB_IDENTITY_TOKEN_FILE))),
      file_watcher_(dispatcher.createFilesystemWatcher()) {

  uri_.set_cluster(config_.cluster());
  uri_.set_uri(config_.uri());
  // TODO: Figure out how to get this to compile, timeout is not all that
  // important right now uri_.set_allocated_timeout(config_.mutable_timeout())

  // AWS_WEB_IDENTITY_TOKEN_FILE and AWS_ROLE_ARN must be set for STS
  // credentials to be enabled
  if (token_file_ == "") {
    throw EnvoyException(fmt::format("Env var {} must be present, and set",
                                     AWS_WEB_IDENTITY_TOKEN_FILE));
  }
  if (default_role_arn_ == "") {
    throw EnvoyException(
        fmt::format("Env var {} must be present, and set", AWS_ROLE_ARN));
  }
  // File must exist on system
  if (!api_.fileSystem().fileExists(token_file_)) {
    throw EnvoyException(
        fmt::format("Web token file {} does not exist", token_file_));
  }

  web_token_ = api_.fileSystem().fileReadToEnd(token_file_);
  // File should not be empty
  if (web_token_ == "") {
    throw EnvoyException(
        fmt::format("Web token file {} exists but is empty", token_file_));
  }
}

void StsCredentialsProviderImpl::init() {
  // Add file watcher for token file
  auto shared_this = shared_from_this();
  file_watcher_->addWatch(
      token_file_, Filesystem::Watcher::Events::Modified,
      [shared_this](uint32_t) {
        try {
          const auto web_token = shared_this->api_.fileSystem().fileReadToEnd(
              shared_this->token_file_);
          // TODO: check if web_token is valid
          // TODO: stats here
          shared_this->web_token_ = web_token;
        } catch (const EnvoyException &e) {
          ENVOY_LOG_TO_LOGGER(
              Envoy::Logger::Registry::getLog(Logger::Id::aws), warn,
              "{}: Exception while reading file during watch ({}): {}",
              __func__, shared_this->token_file_, e.what());
        }
      });
}

void StsCredentialsProviderImpl::onSuccess(
    std::shared_ptr<const StsCredentials> result, std::string_view role_arn) {
  credentials_cache_.emplace(role_arn, result);
}

void StsCredentialsProviderImpl::find(
    const absl::optional<std::string> &role_arn_arg, ContextSharedPtr context) {
  auto &ctximpl = static_cast<Context &>(*context);

  std::string role_arn = default_role_arn_;
  // If role_arn_arg is present, use that, otherwise use env
  if (role_arn_arg.has_value()) {
    role_arn = role_arn_arg.value();
  }

  ASSERT(!role_arn.empty());

  ENVOY_LOG(trace, "{}: Attempting to assume role ({})", __func__, role_arn);

  const auto existing_token = credentials_cache_.find(role_arn);
  if (existing_token != credentials_cache_.end()) {
    // thing  exists
    const auto now = api_.timeSource().systemTime();
    // If the expiration time is more than a minute away, return it immediately
    auto time_left = existing_token->second->expirationTime() - now;
    if (time_left > REFRESH_GRACE_PERIOD) {
      ctximpl.callbacks()->onSuccess(existing_token->second);
      return;
    }
    // token is expired, fallthrough to create a new one
  }

  // Look for active connection pool for given role_arn
  const auto existing_pool = connection_pools_.find(role_arn);
  if (existing_pool != credentials_cache_.end()) {
    // We have an existing connection pool, add new context to connection pool
  }

  // No pool exists, create a new one

  ctximpl.fetcher().fetch(
      uri_, role_arn, web_token_,
      [this, context, role_arn](const absl::string_view body) {
        ASSERT(body != nullptr);

// using a macro as we need to return on error
// TODO(yuval-k): we can use string_view instead of string when we upgrade to
// newer absl.
#define GET_PARAM(X)                                                           \
  std::string X;                                                               \
  {                                                                            \
    std::match_results<absl::string_view::const_iterator> matched;             \
    bool result =                                                              \
        std::regex_search(body.begin(), body.end(), matched, regex_##X##_);    \
    if (!result || !(matched.size() != 1)) {                                   \
      ENVOY_LOG(trace, "response body did not contain " #X);                   \
      context->callbacks()->onFailure(CredentialsFailureStatus::InvalidSts);   \
      return;                                                                  \
    }                                                                          \
    const auto &sub_match = matched[1];                                        \
    decltype(X) matched_sv(sub_match.first, sub_match.length());               \
    X = std::move(matched_sv);                                                 \
  }

        GET_PARAM(access_key);
        GET_PARAM(secret_key);
        GET_PARAM(session_token);
        GET_PARAM(expiration);

        SystemTime expiration_time;
        absl::Time absl_expiration_time;
        std::string error;
        if (absl::ParseTime(absl::RFC3339_sec, expiration,
                            &absl_expiration_time, &error)) {
          ENVOY_LOG(trace,
                    "Determined expiration time from STS credentials result");
          expiration_time = absl::ToChronoTime(absl_expiration_time);
        } else {
          expiration_time = api_.timeSource().systemTime() + REFRESH_STS_CREDS;
          ENVOY_LOG(trace,
                    "Unable to determine expiration time from STS credentials "
                    "result (error: {}), using default",
                    error);
        }

        StsCredentialsConstSharedPtr result =
            std::make_shared<const StsCredentials>(
                access_key, secret_key, session_token, expiration_time);

        // Success callback, save back to cache
        credentials_cache_.emplace(role_arn, result);
        context->callbacks()->onSuccess(result);
      },
      [context](CredentialsFailureStatus reason) {
        // unsuccessful, send back empty creds?
        context->callbacks()->onFailure(reason);
      });
};

StsCredentialsProviderPtr StsCredentialsProviderFactoryImpl::create(
    const envoy::config::filter::http::aws_lambda::v2::
        AWSLambdaConfig_ServiceAccountCredentials &config) const {

  return StsCredentialsProviderImpl::create(config, api_, dispatcher_);
};

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
