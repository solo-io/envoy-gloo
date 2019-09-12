#include "extensions/filters/http/aws_lambda/config.h"

#include "envoy/thread_local/thread_local.h"

#include "extensions/filters/http/common/aws/credentials_provider_impl.h"
#include "extensions/filters/http/common/aws/utility.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {

namespace {
struct ThreadLocalState : public Envoy::ThreadLocal::ThreadLocalObject {
  ThreadLocalState(CredentialsConstSharedPtr credentials)
      : credentials_(credentials) {}
  CredentialsConstSharedPtr credentials_;
};
} // namespace

AWSLambdaConfig::AWSLambdaConfig(
    Api::Api &api, Event::Dispatcher &dispatcher,
    Envoy::ThreadLocal::SlotAllocator &tls,
    const envoy::config::filter::http::aws_lambda::v2::AWSLambdaConfig
        &protoconfig) {
  if (protoconfig.use_instance_metadata()) {
    tls_slot_ = tls.allocateSlot();
    provider_.reset(new Common::Aws::InstanceProfileCredentialsProvider(
        api, HttpFilters::Common::Aws::Utility::metadataFetcher));
    timer_ = dispatcher.createTimer([this] { timerCallback(); });
    timerCallback();
  }
}

AWSLambdaConfig::~AWSLambdaConfig() {}

CredentialsConstSharedPtr AWSLambdaConfig::getCredentials() const {
  if (!provider_) {
    return {};
  }

  // tls_slot_ != nil IFF provider_ != nil
  auto threadState = tls_slot_->get();

  if (!threadState) {
    return {};
  }

  return std::dynamic_pointer_cast<ThreadLocalState>(threadState)->credentials_;
}

void AWSLambdaConfig::timerCallback() {
  // get new credentials.

  CredentialsConstSharedPtr newcreds = std::make_shared<
      Envoy::Extensions::HttpFilters::Common::Aws::Credentials>(
      provider_->getCredentials());

  // set on all threads
  tls_slot_->set([newcreds](Event::Dispatcher &) {
    return std::make_shared<ThreadLocalState>(newcreds);
  });

  // re-enable refersh timer
  constexpr std::chrono::milliseconds delay = std::chrono::hours(1);

  timer_->enableTimer(delay);
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

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
