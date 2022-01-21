#include "source/extensions/filters/http/aws_lambda/sts_chained_fetcher.h"

#include "source/extensions/filters/http/aws_lambda/sts_response_parser.h"

#include "source/extensions/filters/http/aws_lambda/aws_authenticator.h"

#include "source/common/buffer/buffer_impl.h"
#include "source/common/common/enum_to_int.h"
#include "source/common/common/regex.h"
#include "source/common/http/utility.h"
#include "source/common/http/headers.h"



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
      : cm_(cm), api_(api), base_role_arn_(base_role_arn) ,
       aws_authenticator_(api.timeSource(), 
       &AWSStsHeaderNames::get().Service){
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
             const std::string access_key, 
             const std::string secret_key,
             const std::string session_token,
             StsChainedFetcher::ChainedCallback *callback) override {
    ENVOY_LOG(trace, "{}", __func__);
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
        "is not configured", __func__, uri.uri(), uri.cluster());
      complete_ = true;
      callback_->onChainedFailure(CredentialsFailureStatus::ClusterNotFound);
      reset();
      return;
    }

    Http::RequestMessagePtr message = Http::Utility::prepareHeaders(uri);
    // Post with form-encoded works aligns with lambda call implementation
    // If need be this can be converted to GET with query-params
    message->headers().setReferenceMethod(
        Http::Headers::get().MethodValues.Post);
    message->headers().setContentType(
        Http::Headers::get().ContentTypeValues.FormUrlEncoded);
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                         api_.timeSource().systemTime().time_since_epoch())
                         .count();
    const std::string body = fmt::format(StsChainedFormatString, role_arn, now);

    message->body().add(body);

    auto options = Http::AsyncClient::RequestOptions().setTimeout(
                          std::chrono::milliseconds(
                          DurationUtil::durationToMilliseconds(uri.timeout())));
 
    aws_authenticator_.init(&access_key, &secret_key, &session_token);
    aws_authenticator_.updatePayloadHash(message->body());

    ENVOY_LOG(debug, " chained auth payload {}", message->body().length());

    // TODO(nfudenberg) dont do this silly dereference if possible
    auto& hdrs = message->headers();
    // TODO(nfudenberg) allow for Region this to be overridable. 
    // DefaultRegion is gauranteed to be available 
    // Configured override region may be faster.
    aws_authenticator_.sign(&hdrs, HeadersToSign, DefaultRegion);

    request_ = thread_local_cluster->httpAsyncClient().send(
                                            std::move(message), *this, options);
  }

  // HTTP async receive methods
  void onSuccess(const Http::AsyncClient::Request &,
                 Http::ResponseMessagePtr &&response) override {
    complete_ = true;
   
   if (response->body().length() > 0) {
      const auto len = response->body().length();
      const auto body = absl::string_view(
            static_cast<char *>(response->body().linearize(len)), len);

     ENVOY_LOG(debug, "{}: chained body ", body);
      // CONSIDER: moving this to a better function loction
      // ripped from sts_connection
      #define GET_PARAM(X)                                                     \
      std::string X;                                                           \
      {                                                                        \
        std::match_results<absl::string_view::const_iterator> matched;         \
        bool result = std::regex_search(body.begin(), body.end(), matched,     \
                                        DUPEStsResponseRegex::get().regex_##X);\
        if (!result || !(matched.size() != 1)) {                               \
          ENVOY_LOG(trace, "response body did not contain " #X);               \
          callback_-> onChainedFailure(CredentialsFailureStatus::InvalidSts);  \
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
      callback_->onChainedSuccess(access_key, secret_key, 
                                                    session_token, expiration);
   }else{
      callback_->onChainedFailure(CredentialsFailureStatus::Network);
   }



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
  AwsAuthenticator aws_authenticator_;


  class AWSStsHeaderValues {
  public:
    const std::string Service{"sts"};
     const Http::LowerCaseString DateHeader{"x-amz-date"};
    const Http::LowerCaseString FunctionError{"x-amz-function-error"};
  };
  typedef ConstSingleton<AWSStsHeaderValues> AWSStsHeaderNames;
  const HeaderList HeadersToSign =
    AwsAuthenticator::createHeaderToSign(
        { 
          Http::Headers::get().ContentType,
        AWSStsHeaderNames::get().DateHeader,
        Http::Headers::get().HostLegacy}
         );

  // work around having consts being passed around.
  void set_role(const absl::string_view role_arn){
    role_arn_ = role_arn;
  }

  
  const std::string DefaultRegion = "us-east-1";

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
