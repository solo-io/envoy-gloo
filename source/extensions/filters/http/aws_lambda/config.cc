#include "extensions/filters/http/aws_lambda/config.h"

#include "envoy/thread_local/thread_local.h"

#include "common/common/regex.h"

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

struct ThreadLocalCredentials : public Envoy::ThreadLocal::ThreadLocalObject {
  ThreadLocalCredentials(CredentialsConstSharedPtr credentials)
      : credentials_(credentials) {}
  CredentialsConstSharedPtr credentials_;
};

} // namespace

AWSLambdaConfigImpl::AWSLambdaConfigImpl(
    std::unique_ptr<Extensions::Common::Aws::CredentialsProvider> &&provider,
    Upstream::ClusterManager &cluster_manager,
    StsCredentialsProviderFactory &sts_factory, Event::Dispatcher &dispatcher,
    Envoy::ThreadLocal::SlotAllocator &tls, const std::string &stats_prefix,
    Stats::Scope &scope, Api::Api &api,
    const envoy::config::filter::http::aws_lambda::v2::AWSLambdaConfig
        &protoconfig)
    : context_factory_(cluster_manager, api),
      stats_(generateStats(stats_prefix, scope)) {

  // Initialize Credential fetcher, if none exists do nothing. Filter will
  // implicitly use protocol options data
  switch (protoconfig.credentials_fetcher_case()) {
  case envoy::config::filter::http::aws_lambda::v2::AWSLambdaConfig::
      CredentialsFetcherCase::kUseDefaultCredentials: {
    ENVOY_LOG(debug, "{}: Using default credentials source", __func__);
    provider_ = std::move(provider);

    tls_slot_ = tls.allocateSlot();
    auto empty_creds = std::make_shared<const CommonAws::Credentials>();
    tls_slot_->set([empty_creds](Event::Dispatcher &) {
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
    // use service account credentials provider
    auto service_account_creds = protoconfig.service_account_credentials();
    sts_credentials_provider_ = sts_factory.create(service_account_creds);
    break;
  }
  case envoy::config::filter::http::aws_lambda::v2::AWSLambdaConfig::
      CredentialsFetcherCase::CREDENTIALS_FETCHER_NOT_SET: {
    break;
  }
  }
}

/*
 * Three options, in order of precedence
 *   1. Protocol Options
 *   2. Default Provider
 *   3. STS
 */
ContextSharedPtr AWSLambdaConfigImpl::getCredentials(
    SharedAWSLambdaProtocolExtensionConfig ext_cfg,
    StsCredentialsProvider::Callbacks *callbacks) const {
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
    callbacks->onSuccess(
        tls_slot_->getTyped<ThreadLocalCredentials>().credentials_);
    // no context necessary as these credentials are available immediately
    return nullptr;
  }

  if (sts_credentials_provider_) {
    ENVOY_LOG(trace, "{}: Credentials being retrieved from STS provider",
              __func__);
    // return the context directly to the filter, as no direct credentials can
    // be sent
    auto context = context_factory_.create(callbacks);
    sts_credentials_provider_->find(ext_cfg->roleArn(), context);
    return context;
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
        tls_slot_->getTyped<ThreadLocalCredentials>().credentials_;
    if (currentCreds == nullptr || !((*currentCreds) == new_creds)) {
      stats_.creds_rotated_.inc();
      ENVOY_LOG(debug, "refreshing AWS credentials");
      auto shared_new_creds =
          std::make_shared<const CommonAws::Credentials>(new_creds);
      tls_slot_->set([shared_new_creds](Event::Dispatcher &) {
        return std::make_shared<ThreadLocalCredentials>(shared_new_creds);
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
}

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
