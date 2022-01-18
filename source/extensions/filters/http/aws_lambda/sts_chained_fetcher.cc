#include "source/extensions/filters/http/aws_lambda/sts_chained_fetcher.h"

#include "source/common/buffer/buffer_impl.h"
#include "source/common/common/enum_to_int.h"
#include "source/common/common/regex.h"
#include "source/common/http/headers.h"
#include "source/common/http/utility.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {

namespace {

class StsChainedFetcherImpl : public StsChainedFetcher,
                       public Logger::Loggable<Logger::Id::aws>,
                       public Http::AsyncClient::Callbacks {
public:
  StsChainedFetcherImpl(Upstream::ClusterManager &cm, Api::Api &api, 
                                          const absl::string_view base_role_arn)
      : cm_(cm), api_(api), base_role_arn_(base_role_arn) {
    ENVOY_LOG(trace, "{}", __func__);
  }

  ~StsChainedFetcherImpl() override { cancel(); }

  void cancel() override {
    if (request_ && !complete_) {
      request_->cancel();
      ENVOY_LOG(debug, "assume role with token [uri = {}]: canceled",
                uri_->uri());
    }
    reset();
  }

  void fetch(const envoy::config::core::v3::HttpUri &uri,
             const  absl::string_view role_arn,
             const  absl::string_view webtoken_response,
             StsChainedFetcher::ChainedCallback *callback) override {
    ENVOY_LOG(trace, "{}", __func__);
    
    ASSERT(!webtoken_response.empty());
    complete_ = false;
    callback_ = callback;
    uri_ = &uri;
    set_role(role_arn);


    // Check if cluster is configured, fail the request if not.
    // Otherwise cm_.httpAsyncClientForCluster will throw exception.
    const auto thread_local_cluster = cm_.getThreadLocalCluster(uri.cluster());
    
    if (thread_local_cluster == nullptr) {
      ENVOY_LOG(error,
                "{}: chained assume role [uri = {}] failed: [cluster = {}] "
                "is not configured",
                __func__, uri.uri(), uri.cluster());
      complete_ = true;
      callback_->onChainedFailure(CredentialsFailureStatus::ClusterNotFound);
      reset();
      return;
    }


    //TODO: REMOVE THIS TESTING ONLY
    if (true){
      // callback_->onChainedSuccess(webtoken_response);
      return;
    }

    Http::RequestMessagePtr message = Http::Utility::prepareHeaders(uri);
    message->headers().setReferenceMethod(
        Http::Headers::get().MethodValues.Post);
    message->headers().setContentType(
        Http::Headers::get().ContentTypeValues.FormUrlEncoded);
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                         api_.timeSource().systemTime().time_since_epoch())
                         .count();
    const std::string body =
        fmt::format(StsChainedFormatString, role_arn, now);
    message->body().add(body);
    ENVOY_LOG(debug, "assume role with token from [uri = {}]: start",
              uri_->uri());
    auto options = Http::AsyncClient::RequestOptions().setTimeout(
        std::chrono::milliseconds(
            DurationUtil::durationToMilliseconds(uri.timeout())));
    request_ = thread_local_cluster->httpAsyncClient().send(std::move(message), *this, options);
  }

  // HTTP async receive methods
  void onSuccess(const Http::AsyncClient::Request &,
                 Http::ResponseMessagePtr &&response) override {
    complete_ = true;
   
   if (response->body().length() > 0) {
      const auto len = response->body().length();
        const auto body = absl::string_view(
            static_cast<char *>(response->body().linearize(len)), len);

     ENVOY_LOG(debug, "{}: chained ", body);
   }


    callback_->onChainedFailure(CredentialsFailureStatus::Network);
    
    reset();
  }

  void onFailure(const Http::AsyncClient::Request &,
                 Http::AsyncClient::FailureReason reason) override {
    complete_ = true;
    ENVOY_LOG(debug, "{}: chained assume role [uri = {}]: network error {}",
              __func__, uri_->uri(), enumToInt(reason));
    callback_->onChainedFailure(CredentialsFailureStatus::Network);
    
    reset();
  }

  void onBeforeFinalizeUpstreamSpan(Tracing::Span &,
                                    const Http::ResponseHeaderMap *) override {}

private:
  Upstream::ClusterManager &cm_;
  Api::Api &api_;
  const absl::string_view base_role_arn_;
  bool complete_{};
  StsChainedFetcher::ChainedCallback *callback_{};
  const envoy::config::core::v3::HttpUri *uri_{};
  Http::AsyncClient::Request *request_{};
  absl::string_view role_arn_;

  // work around having consts being passed around.
  void set_role(const absl::string_view role_arn){
    role_arn_ = role_arn;
  }

  void reset() {
    request_ = nullptr;
    callback_ = nullptr;
    uri_ = nullptr;
  }
};
} // namespace

StsChainedFetcherPtr StsChainedFetcher::create(Upstream::ClusterManager &cm, Api::Api &api,
                                             const absl::string_view base_role_arn) {
  return std::make_unique<StsChainedFetcherImpl>(cm, api, base_role_arn);
}

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
