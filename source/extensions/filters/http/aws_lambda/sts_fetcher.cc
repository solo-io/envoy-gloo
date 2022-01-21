#include "source/extensions/filters/http/aws_lambda/sts_fetcher.h"

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

class StsFetcherImpl : public StsFetcher,
                       public Logger::Loggable<Logger::Id::aws>,
                       public Http::AsyncClient::Callbacks,
                       public StsChainedFetcher::ChainedCallback {
public:
  StsFetcherImpl(Upstream::ClusterManager &cm, Api::Api &api, 
                                          const absl::string_view base_role_arn)
      : cm_(cm), api_(api), base_role_arn_(base_role_arn),
      chained_fetcher_(StsChainedFetcher::create(cm, api, base_role_arn)) {
    ENVOY_LOG(trace, "{}", __func__);
  }

  ~StsFetcherImpl() override { cancel(); }

  void cancel() override {
    if (request_ && !complete_) {
      request_->cancel();
      ENVOY_LOG(debug, "assume role with token [uri = {}]: canceled",
                uri_->uri());
    }
    if (chained_fetcher_ != nullptr && !complete_){    
      chained_fetcher_->cancel();
    }
    reset();
  }

  void fetch(const envoy::config::core::v3::HttpUri &uri,
             const  absl::string_view role_arn,
             const absl::string_view web_token,
             StsFetcher::Callbacks *callbacks) override {
    ENVOY_LOG(trace, "{}", __func__);
    ASSERT(callbacks_ == nullptr);

    complete_ = false;
    callbacks_ = callbacks;
    uri_ = &uri;
    set_role(role_arn);
    

    // Check if cluster is configured, fail the request if not.
    // Otherwise cm_.httpAsyncClientForCluster will throw exception.
    const auto thread_local_cluster = cm_.getThreadLocalCluster(uri.cluster());
    
    if (thread_local_cluster == nullptr) {
      ENVOY_LOG(error,
                "{}: assume role with token [uri = {}] failed: [cluster = {}] "
                "is not configured",
                __func__, uri.uri(), uri.cluster());
      complete_ = true;
      callbacks_->onFailure(CredentialsFailureStatus::ClusterNotFound);
      reset();
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
        fmt::format(StsFormatString, base_role_arn_, now, web_token);
    message->body().add(body);
    ENVOY_LOG(debug, "assume role with token from [uri = {}]: start",
              uri_->uri());
    auto options = Http::AsyncClient::RequestOptions().setTimeout(
        std::chrono::milliseconds(
            DurationUtil::durationToMilliseconds(uri.timeout())));
    request_ = thread_local_cluster->httpAsyncClient().send(std::move(message),
                                                                *this, options);
  }

  // HTTP async receive methods
  void onSuccess(const Http::AsyncClient::Request &,
                 Http::ResponseMessagePtr &&response) override {
    complete_ = true;
    const uint64_t status_code =
        Http::Utility::getResponseStatus(response->headers());
    if (status_code == enumToInt(Http::Code::OK)) {
      ENVOY_LOG(debug, "{}: assume role with token [uri = {}]: success",
                __func__, uri_->uri());
      if (response->body().length() > 0) {
        const auto len = response->body().length();
        const auto body = absl::string_view(
            static_cast<char *>(response->body().linearize(len)), len);

      // CONSIDER: moving this to a better function loction
      // ripped from sts_connection
      #define GET_PARAM(X)                                                     \
      std::string X;                                                           \
      {                                                                        \
        std::match_results<absl::string_view::const_iterator> matched;         \
        bool result = std::regex_search(body.begin(), body.end(), matched,     \
                                      StsResponseRegex::get().regex_##X);  \
        if (!result || !(matched.size() != 1)) {                               \
          ENVOY_LOG(trace, "response body did not contain " #X);               \
          onChainedFailure(CredentialsFailureStatus::InvalidSts);              \
          return;                                                              \
        }                                                                      \
        const auto &sub_match = matched[1];                                    \
        decltype(X) matched_sv(sub_match.first, sub_match.length());           \
        X = std::move(matched_sv);                                             \
      }

      GET_PARAM(access_key);
      GET_PARAM(secret_key);
      GET_PARAM(session_token);
      GET_PARAM(expiration);

        // For the default user (ie the one on the annotation
        // no need for chaining so return as is
        if (role_arn_ == base_role_arn_){
          
          onChainedSuccess(access_key, secret_key, session_token, expiration);
        }else{
          chained_fetcher_->fetch(*uri_, role_arn_, access_key, secret_key,
                                                         session_token,  this);
        }

      } else {
        ENVOY_LOG(debug, "{}: assume role with token [uri = {}]: body is empty",
                  __func__, uri_->uri());
        onChainedFailure(CredentialsFailureStatus::Network);
      }
    } else {
      if ((status_code >= 400) && (status_code <= 403) && (response->body().length() > 0)) {
        const auto len = response->body().length();
        const auto body = absl::string_view(
            static_cast<char *>(response->body().linearize(len)), len);
        ENVOY_LOG(debug, "{}: StatusCode: {}, Body: \n {}", __func__,
                  status_code, body);
        // TODO: cover more AWS error cases
        if (body.find(ExpiredTokenError) != std::string::npos) {
          onChainedFailure(CredentialsFailureStatus::ExpiredToken);
        } else {
          onChainedFailure(CredentialsFailureStatus::Network);
        }
        // TODO: parse the error string. Example:
        /*
          <ErrorResponse
          xmlns="http://webservices.amazon.com/AWSFault/2005-15-09"> <Error>
              <Type>Sender</Type>
              <Code>InvalidAction</Code>
              <Message>Could not find operation AssumeRoleWithWebIdentity for
          version NO_VERSION_SPECIFIED</Message>
            </Error>
            <RequestId>72168399-bcdd-4248-bf57-bf5d4a6dc07d</RequestId>
          </ErrorResponse>
        */
      } else {
        ENVOY_LOG(
            debug,
            "{}: assume role with token [uri = {}]: response status code {}",
            __func__, uri_->uri(), status_code);
        ENVOY_LOG(trace, "{}: headers: {}", __func__, response->headers());
        onChainedFailure(CredentialsFailureStatus::Network);
      }
    }

  }

  void onFailure(const Http::AsyncClient::Request &,
                 Http::AsyncClient::FailureReason reason) override {
    ENVOY_LOG(debug, "{}: assume role with token [uri = {}]: network error {}",
              __func__, uri_->uri(), enumToInt(reason));
    onChainedFailure(CredentialsFailureStatus::Network);
   
  }


    // HTTP async receive methods
  void onChainedSuccess(const std::string access_key, 
        const std::string secret_key, const std::string session_token,
                                              const std::string expiration)  {
    complete_ = true;
    SystemTime expiration_time;
    absl::Time absl_expiration_time;
    std::string error;
    if (absl::ParseTime(absl::RFC3339_sec, expiration, &absl_expiration_time,
                        &error)) {
      ENVOY_LOG(trace, "Determined expiration time via STS credentials result");
      expiration_time = absl::ToChronoTime(absl_expiration_time);
    } else {
      expiration_time = api_.timeSource().systemTime() + DUPE_REFRESH_STS_CREDS;
      ENVOY_LOG(trace,
                "Unable to determine expiration time from STS credentials "
                "result (error: {}), using default",
                error);
    }
    callbacks_->onSuccess(access_key,secret_key,session_token, expiration_time);
    reset();
  }

  void onChainedFailure(CredentialsFailureStatus reason)  {
    complete_ = true;
    callbacks_->onFailure(reason);
     reset();
  }



  void onBeforeFinalizeUpstreamSpan(Tracing::Span &,
                                    const Http::ResponseHeaderMap *) override {}

private:
  Upstream::ClusterManager &cm_;
  Api::Api &api_;
  const absl::string_view base_role_arn_;
  bool complete_{};
  StsFetcher::Callbacks *callbacks_{};
  const envoy::config::core::v3::HttpUri *uri_{};
  Http::AsyncClient::Request *request_{};
  absl::string_view role_arn_;
  StsChainedFetcherPtr chained_fetcher_;

  // work around having consts being passed around.
  void set_role(const absl::string_view role_arn){
    role_arn_ = role_arn;
  }

  void reset() {
    request_ = nullptr;
    callbacks_ = nullptr;
    uri_ = nullptr;
  }
};
} // namespace

StsFetcherPtr StsFetcher::create(Upstream::ClusterManager &cm, Api::Api &api,
                                        const absl::string_view base_role_arn) {
  return std::make_unique<StsFetcherImpl>(cm, api, base_role_arn);
}

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
