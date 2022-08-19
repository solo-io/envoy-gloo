#include "source/extensions/filters/http/aws_lambda/config.h"

#include "source/extensions/filters/http/transformation/transformation_filter_config.h"

#include "envoy/thread_local/thread_local.h"

#include "source/common/common/regex.h"
#include "source/common/config/utility.h"

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
} // namespace

AWSLambdaConfigImpl::AWSLambdaConfigImpl(
    std::unique_ptr<Extensions::Common::Aws::CredentialsProvider> &&provider,
    std::unique_ptr<StsCredentialsProviderFactory> &&sts_factory,
    Event::Dispatcher &dispatcher, Api::Api &api,
    Envoy::ThreadLocal::SlotAllocator &tls, const std::string &stats_prefix,
    Stats::Scope &scope,
    const envoy::config::filter::http::aws_lambda::v2::AWSLambdaConfig
        &protoconfig)
    : stats_(generateStats(stats_prefix, scope)), api_(api),
      file_watcher_(dispatcher.createFilesystemWatcher()), tls_(tls),
      credential_refresh_delay_(std::chrono::milliseconds(
        DurationUtil::durationToMilliseconds(
          protoconfig.credential_refresh_delay()))),
          propagate_original_routing_(protoconfig.propagate_original_routing()){


  // Initialize Credential fetcher, if none exists do nothing. Filter will
  // implicitly use protocol options data
  switch (protoconfig.credentials_fetcher_case()) {
  case envoy::config::filter::http::aws_lambda::v2::AWSLambdaConfig::
      CredentialsFetcherCase::kUseDefaultCredentials: {
    ENVOY_LOG(debug, "{}: Using default credentials source", __func__);
    provider_ = std::move(provider);

    auto empty_creds = std::make_shared<const CommonAws::Credentials>();
    tls_.set([empty_creds](Event::Dispatcher &) {
      return std::make_shared<ThreadLocalCredentials>(empty_creds);
    });

    timer_ = dispatcher.createTimer([this] { timerCallback(); });
    // call the time callback to fetch credentials now.
    // this will also re-trigger the timer.
    timerCallback();
    break;
  }
  case envoy::config::filter::http::aws_lambda::v2::AWSLambdaConfig::
      CredentialsFetcherCase::kServiceAccountCredentials: {
    ENVOY_LOG(debug, "{}: Using STS credentials source", __func__);

    // Load all of the env data for STS credentials
    loadSTSData();
    // use service account credentials provider
    // transfer ptr ownership to sts_factor isn't cleaned up before we get into
    // tls set
    sts_factory_ = std::move(sts_factory);
    auto service_account_creds = protoconfig.service_account_credentials();
    tls_.set([this, web_token = web_token_, role_arn = role_arn_,
                    service_account_creds](Event::Dispatcher &dispatcher) {
      StsCredentialsProviderPtr sts_cred_provider = sts_factory_->build(
          service_account_creds, dispatcher, web_token, role_arn);
      return std::make_shared<ThreadLocalCredentials>(
          std::move(sts_cred_provider));
    });
    sts_enabled_ = true;
    break;
  }
  case envoy::config::filter::http::aws_lambda::v2::AWSLambdaConfig::
      CredentialsFetcherCase::CREDENTIALS_FETCHER_NOT_SET: {
    break;
  }
  }
}

void AWSLambdaConfigImpl::loadSTSData() {
  // AWS_WEB_IDENTITY_TOKEN_FILE and AWS_ROLE_ARN must be set for STS
  // credentials to be enabled
  token_file_ =
      std::string(absl::NullSafeStringView(std::getenv(AWS_WEB_IDENTITY_TOKEN_FILE)));
  if (token_file_ == "") {
    throw EnvoyException(fmt::format("Env var {} must be present, and set",
                                     AWS_WEB_IDENTITY_TOKEN_FILE));
  }
  role_arn_ = std::string(absl::NullSafeStringView(std::getenv(AWS_ROLE_ARN)));
  if (role_arn_ == "") {
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

void AWSLambdaConfigImpl::init(Event::Dispatcher &dispatcher) {
  if (sts_enabled_) {
    // Set up a filewatch and timer for sts token upating.
    auto shared_this = shared_from_this();

    // While the filewatch should be sufficient we have seen instances where the calls are dropped.
    // Given the usual usage of sts this should only be of concern when web token is self managed.
    shared_this->timer_ = dispatcher.createTimer([shared_this] { 
        try {
            const auto web_token = shared_this->api_.fileSystem().fileReadToEnd(
                shared_this->token_file_);
             shared_this->stats_.webtoken_rotated_.inc();
             // We enforce that it should not be empty at start up
             // but are more lenient at this point.
            if (web_token == "") {
              shared_this->stats_.webtoken_failure_.inc();
            }else{
              shared_this->tls_.runOnAllThreads(
                  [web_token](OptRef<ThreadLocalCredentials> prev_config) {
                    prev_config->sts_credentials_->setWebToken(web_token);
                  });
            }
            // TODO: check if web_token is valid
            // TODO: stats here
          } catch (const EnvoyException &e) {
            ENVOY_LOG_TO_LOGGER(
                Envoy::Logger::Registry::getLog(Logger::Id::aws), warn,
                "{}: Exception while reading web token file ({}): {}",
                __func__, shared_this->token_file_, e.what());
          }

        // re-enable refresh te timer with paranoid short time
        // Time decided given min plausible web_token life of 1 hour in AWS
        if (shared_this->credential_refresh_delay_.count() > 0){
            shared_this->timer_->enableTimer(
                                shared_this->credential_refresh_delay_);
        }
     });
    if (credential_refresh_delay_.count() > 0){
          ENVOY_LOG(debug, "{}: STS enabled with {} time refresh",
       __func__, shared_this->credential_refresh_delay_.count());
      shared_this->timer_->enableTimer(shared_this->credential_refresh_delay_);
    } else {
        ENVOY_LOG(debug, "{}: STS enabled without time based refresh",__func__);
    }
    file_watcher_->addWatch(
        token_file_, Filesystem::Watcher::Events::Modified,
        [shared_this](uint32_t) {
          // Force timer callback to happen immediately to pick up the change.
          shared_this->timer_->enableTimer(std::chrono::milliseconds::zero());
        });
  }
}

/*
 * Three options, in order of precedence
 *   1. Protocol Options
 *   2. Default Provider
 *   3. STS
 */
StsConnectionPool::Context *AWSLambdaConfigImpl::getCredentials(
    SharedAWSLambdaProtocolExtensionConfig ext_cfg,
    StsConnectionPool::Context::Callbacks *callbacks) const {
  // Always check extension config first, as it overrides
  if (ext_cfg->accessKey().has_value() && ext_cfg->secretKey().has_value()) {
    ENVOY_LOG(trace, "{}: Credentials found from protocol options", __func__);
    // attempt to set session_token, ok if nil
    if (ext_cfg->sessionToken().has_value()) {
      callbacks->onSuccess(
          std::make_shared<const Envoy::Extensions::Common::Aws::Credentials>(
              ext_cfg->accessKey().value(), ext_cfg->secretKey().value(),
              ext_cfg->sessionToken().value()));
    } else {
      callbacks->onSuccess(
          std::make_shared<const Envoy::Extensions::Common::Aws::Credentials>(
              ext_cfg->accessKey().value(), ext_cfg->secretKey().value()));
    }
    return nullptr;
  }

  if (provider_) {
    ENVOY_LOG(trace, "{}: Credentials found from default source", __func__);
    callbacks->onSuccess(tls_->credentials_);
    // no context necessary as these credentials are available immediately
    return nullptr;
  }

  if (sts_enabled_) {
    ENVOY_LOG(trace, "{}: Credentials being retrieved from STS provider",
              __func__);
    return tls_->sts_credentials_->find(ext_cfg->roleArn(),
                             ext_cfg->disableRoleChaining(), callbacks);
  }

  ENVOY_LOG(debug, "{}: No valid credentials source found", __func__);
  callbacks->onFailure(CredentialsFailureStatus::InvalidSts);
  return nullptr;
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
    auto currentCreds =
        tls_->credentials_;
    if (currentCreds == nullptr || !((*currentCreds) == new_creds)) {
      stats_.creds_rotated_.inc();
      ENVOY_LOG(debug, "refreshing AWS credentials");
      auto shared_new_creds =
          std::make_shared<const CommonAws::Credentials>(new_creds);
      tls_.set([shared_new_creds](Event::Dispatcher &) {
        return std::make_shared<ThreadLocalCredentials>(shared_new_creds);
      });
    }
  }

  if (timer_ != nullptr) {
    // re-enable refersh timer
    timer_->enableTimer(REFRESH_AWS_CREDS);
  }
}

std::shared_ptr<AWSLambdaConfigImpl> AWSLambdaConfigImpl::create(
    std::unique_ptr<Envoy::Extensions::Common::Aws::CredentialsProvider>
        &&provider,
    std::unique_ptr<StsCredentialsProviderFactory> &&sts_factory,
    Event::Dispatcher &dispatcher, Api::Api &api,
    Envoy::ThreadLocal::SlotAllocator &tls, const std::string &stats_prefix,
    Stats::Scope &scope,
    const envoy::config::filter::http::aws_lambda::v2::AWSLambdaConfig
        &protoconfig) {
  // We can't use make_shared here because the constructor of this class is
  // private.
  std::shared_ptr<AWSLambdaConfigImpl> ptr(new AWSLambdaConfigImpl(
      std::move(provider), std::move(sts_factory), dispatcher, api, tls,
      stats_prefix, scope, protoconfig));
  ptr->init(dispatcher);
  return ptr;
}

AwsLambdaFilterStats
AWSLambdaConfigImpl::generateStats(const std::string &prefix,
                                   Stats::Scope &scope) {
  const std::string final_prefix = prefix + "aws_lambda.";
  return {ALL_AWS_LAMBDA_FILTER_STATS(POOL_COUNTER_PREFIX(scope, final_prefix),
                                      POOL_GAUGE_PREFIX(scope, final_prefix))};
}

AWSLambdaRouteConfig::AWSLambdaRouteConfig(
    const envoy::config::filter::http::aws_lambda::v2::AWSLambdaPerRoute &protoconfig,
    Server::Configuration::ServerFactoryContext &context
    )
    : path_(functionUrlPath(protoconfig.name(), protoconfig.qualifier())),
      async_(protoconfig.async()),
      unwrap_as_alb_(protoconfig.unwrap_as_alb()),
      has_transformer_config_(protoconfig.has_transformer_config())
    {

  if (protoconfig.has_empty_body_override()) {
    default_body_ = protoconfig.empty_body_override().value();
  }

  if (protoconfig.has_transformer_config()) {
    auto &factory = Config::Utility::getAndCheckFactory<Transformation::TransformerExtensionFactory>(protoconfig.transformer_config());
    auto config = Config::Utility::translateAnyToFactoryConfig(protoconfig.transformer_config().typed_config(), context.messageValidationContext().staticValidationVisitor(), factory);
    transformer_config_ = factory.createTransformer(*config, context);
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

AWSLambdaProtocolExtensionConfig::AWSLambdaProtocolExtensionConfig(
    const envoy::config::filter::http::aws_lambda::v2::
        AWSLambdaProtocolExtension &protoconfig)
    : host_(protoconfig.host()), region_(protoconfig.region()) {
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
  disable_role_chaining_ = protoconfig.disable_role_chaining();
}

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
