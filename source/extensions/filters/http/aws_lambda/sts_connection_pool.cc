#include "extensions/filters/http/aws_lambda/sts_connection_pool.h"

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

class StsConnectionPoolImpl : public StsConnectionPool,
                              public StsFetcher::Callbacks,
                              public Logger::Loggable<Logger::Id::aws> {
public:
  StsConnectionPoolImpl(Api::Api &api, Event::Dispatcher &dispatcher,
                        const absl::string_view role_arn,
                        StsConnectionPool::Callbacks *callbacks,
                        StsFetcherPtr fetcher);

  ~StsConnectionPoolImpl();

  void init(const envoy::config::core::v3::HttpUri &uri,
            const absl::string_view web_token) override;

  StsConnectionPool::Context *
  add(StsConnectionPool::Context::Callbacks *callbacks) override;

  void onSuccess(const absl::string_view body) override;

  void onFailure(CredentialsFailureStatus status) override;

  bool requestInFlight() override { return request_in_flight_; };

private:
  class ContextImpl : public StsConnectionPool::Context,
                      public Event::DeferredDeletable,
                      public Envoy::LinkedObject<ContextImpl> {
  public:
    ContextImpl(StsConnectionPool::Context::Callbacks *callbacks,
                StsConnectionPoolImpl &parent, Event::Dispatcher &dispatcher)
        : callbacks_(callbacks), parent_(parent), dispatcher_(dispatcher) {}

    StsConnectionPool::Context::Callbacks *callbacks() const override {
      return callbacks_;
    }

    void cancel() override {
      // Cancel should never be called once the context has been removed from
      // the list.
      ASSERT(inserted());
      if (inserted()) {
        dispatcher_.deferredDelete(removeFromList(parent_.connection_list_));
      }
    }

  private:
    StsConnectionPool::Context::Callbacks *callbacks_;
    StsConnectionPoolImpl &parent_;
    Event::Dispatcher &dispatcher_;
  };

  using ContextImplPtr = std::unique_ptr<ContextImpl>;

  StsFetcherPtr fetcher_;
  Api::Api &api_;
  Event::Dispatcher &dispatcher_;
  std::string role_arn_;
  StsConnectionPool::Callbacks *callbacks_;

  std::list<ContextImplPtr> connection_list_;

  bool request_in_flight_ = false;
};

// using ContextImplPtr = std::unique_ptr<StsConnectionPoolImpl::ContextImpl>;

StsConnectionPoolImpl::StsConnectionPoolImpl(
    Api::Api &api, Event::Dispatcher &dispatcher,
    const absl::string_view role_arn, StsConnectionPool::Callbacks *callbacks,
    StsFetcherPtr fetcher)
    : fetcher_(std::move(fetcher)), api_(api), dispatcher_(dispatcher),
      role_arn_(role_arn), callbacks_(callbacks){};

StsConnectionPoolImpl::~StsConnectionPoolImpl() {
  // When the conn pool is being destructed, make sure to inform all of the
  // contexts
  for (auto &&ctx : connection_list_) {
    ctx->callbacks()->onFailure(CredentialsFailureStatus::ContextCancelled);
  }
  // Cancel fetch
  if (fetcher_ != nullptr) {
    fetcher_->cancel();
  }
};

void StsConnectionPoolImpl::init(const envoy::config::core::v3::HttpUri &uri,
                                 const absl::string_view web_token) {
  request_in_flight_ = true;
  fetcher_->fetch(uri, role_arn_, web_token, this);
}

StsConnectionPool::Context *
StsConnectionPoolImpl::add(StsConnectionPool::Context::Callbacks *callbacks) {
  ContextImpl *ctx_ptr{new ContextImpl(callbacks, *this, dispatcher_)};
  std::unique_ptr<ContextImpl> ctx{ctx_ptr};

  LinkedList::moveIntoList(std::move(ctx), connection_list_);
  return ctx_ptr;
};

void StsConnectionPoolImpl::onSuccess(const absl::string_view body) {
  ASSERT(body != nullptr);
  request_in_flight_ = false;

// using a macro as we need to return on error
// TODO(yuval-k): we can use string_view instead of string when we upgrade to
// newer absl.
#define GET_PARAM(X)                                                           \
  std::string X;                                                               \
  {                                                                            \
    std::match_results<absl::string_view::const_iterator> matched;             \
    bool result = std::regex_search(body.begin(), body.end(), matched,         \
                                    StsResponseRegex::get().regex_##X);        \
    if (!result || !(matched.size() != 1)) {                                   \
      ENVOY_LOG(trace, "response body did not contain " #X);                   \
      for (auto &&ctx : connection_list_) {                                    \
        ctx->callbacks()->onFailure(CredentialsFailureStatus::InvalidSts);     \
      }                                                                        \
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
  if (absl::ParseTime(absl::RFC3339_sec, expiration, &absl_expiration_time,
                      &error)) {
    ENVOY_LOG(trace, "Determined expiration time from STS credentials result");
    expiration_time = absl::ToChronoTime(absl_expiration_time);
  } else {
    expiration_time = api_.timeSource().systemTime() + REFRESH_STS_CREDS;
    ENVOY_LOG(trace,
              "Unable to determine expiration time from STS credentials "
              "result (error: {}), using default",
              error);
  }

  StsCredentialsConstSharedPtr result = std::make_shared<const StsCredentials>(
      access_key, secret_key, session_token, expiration_time);

  // Send result back to Credential Provider to store in cache
  callbacks_->onSuccess(result, role_arn_);
  // Send result back to all contexts waiting in list
  while (!connection_list_.empty()) {
    connection_list_.back()->callbacks()->onSuccess(result);
    connection_list_.pop_back();
  }
};

void StsConnectionPoolImpl::onFailure(CredentialsFailureStatus status) {
  request_in_flight_ = false;

  while (!connection_list_.empty()) {
    connection_list_.back()->callbacks()->onFailure(status);
    connection_list_.pop_back();
  }
};

StsConnectionPoolPtr
StsConnectionPool::create(Api::Api &api, Event::Dispatcher &dispatcher,
                          const absl::string_view role_arn,
                          StsConnectionPool::Callbacks *callbacks,
                          StsFetcherPtr fetcher) {

  return std::make_shared<StsConnectionPoolImpl>(api, dispatcher, role_arn,
                                                 callbacks, std::move(fetcher));
}

class StsConnectionPoolFactoryImpl: public StsConnectionPoolFactory {
public:
  StsConnectionPoolFactoryImpl(Api::Api &api, Event::Dispatcher &dispatcher):
    api_(api), dispatcher_(dispatcher) {}

  StsConnectionPoolPtr build(const absl::string_view role_arn,
                                     StsConnectionPool::Callbacks *callbacks,
                                     StsFetcherPtr fetcher) override {

    return StsConnectionPool::create(api_, dispatcher_, role_arn, callbacks, std::move(fetcher));
  };

private:
  Api::Api &api_;
  Event::Dispatcher &dispatcher_;
};

StsConnectionPoolFactoryPtr StsConnectionPoolFactory::create(Api::Api &api,
                                     Event::Dispatcher &dispatcher) {
  return std::make_unique<StsConnectionPoolFactoryImpl>(api, dispatcher);
}

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
