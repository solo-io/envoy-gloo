#include "extensions/filters/http/aws_lambda/sts_fetcher.h"

#include "common/common/enum_to_int.h"
#include "common/common/regex.h"
#include "common/http/headers.h"
#include "common/http/utility.h"
#include "common/http/utility.h"
#include "common/buffer/buffer_impl.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {

namespace {

class StsFetcherImpl :  public StsFetcher,
                        public Logger::Loggable<Logger::Id::aws>,
                        public Http::AsyncClient::Callbacks {
public:
  StsFetcherImpl(Upstream::ClusterManager& cm, Api::Api& api) : cm_(cm), api_(api) { ENVOY_LOG(trace, "{}", __func__); }

  ~StsFetcherImpl() override { cancel(); }

  void cancel() override {
    if (request_ && !complete_) {
      request_->cancel();
      ENVOY_LOG(debug, "assume role with token [uri = {}]: canceled", uri_->uri());
    }
    reset();
  }

  void fetch(const envoy::config::core::v3::HttpUri& uri,
              const absl::string_view role_arn,
              const absl::string_view web_token,
              SuccessCallback success, FailureCallback failure) override {
    ENVOY_LOG(trace, "{}", __func__);
    ASSERT(!success_callback_);
    ASSERT(!failure_callback_);

    complete_ = false;
    success_callback_ = success;
    failure_callback_ = failure;
    uri_ = &uri;

    // Check if cluster is configured, fail the request if not.
    // Otherwise cm_.httpAsyncClientForCluster will throw exception.
    if (cm_.get(uri.cluster()) == nullptr) {
      ENVOY_LOG(error, "{}: assume role with token [uri = {}] failed: [cluster = {}] is not configured",
                __func__, uri.uri(), uri.cluster());
      complete_ = true;
      failure_callback_(CredentialsFailureStatus::ClusterNotFound);
      reset();
      return;
    }

    Http::RequestMessagePtr message = Http::Utility::prepareHeaders(uri);
    message->headers().setReferenceMethod(Http::Headers::get().MethodValues.Post);
    message->headers().setContentType(Http::Headers::get().ContentTypeValues.FormUrlEncoded);
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(api_.timeSource().systemTime().time_since_epoch()).count();
    // TODO: url-encode the body
    const std::string body = fmt::format(StsFormatString, role_arn, now, web_token);
    message->body() = std::make_unique<Buffer::OwnedImpl>(body);
    message->headers().setContentLength(body.length());
    ENVOY_LOG(debug, "assume role with token from [uri = {}]: start", uri_->uri());
    auto options = Http::AsyncClient::RequestOptions()
                       .setTimeout(std::chrono::milliseconds(
                           DurationUtil::durationToMilliseconds(uri.timeout())));
    request_ =
        cm_.httpAsyncClientForCluster(uri.cluster()).send(std::move(message), *this, options);
  }

  // HTTP async receive methods 
  void onSuccess(const Http::AsyncClient::Request&, Http::ResponseMessagePtr&& response) override {
    complete_ = true;
    const uint64_t status_code = Http::Utility::getResponseStatus(response->headers());
    if (status_code == enumToInt(Http::Code::OK)) {
      ENVOY_LOG(debug, "{}: assume role with token [uri = {}]: success", __func__, uri_->uri());
      if (response->body()) {
        const auto len = response->body()->length();
        const auto body = std::string(static_cast<char*>(response->body()->linearize(len)), len);
        success_callback_(&body);

      } else {
        ENVOY_LOG(debug, "{}: assume role with token [uri = {}]: body is empty", __func__, uri_->uri());
        failure_callback_(CredentialsFailureStatus::Network);
      }
    } else {
      if ((status_code % 400) <= 3 && response->body()) {
        const auto len = response->body()->length();
        const auto body = std::string(static_cast<char*>(response->body()->linearize(len)), len);
        ENVOY_LOG(debug, "{}: StatusCode: {}, Body: \n {}", __func__, status_code, body);
        // TODO: cover more AWS error cases
        if (body.find(ExpiredTokenError) != std::string::npos) {
          failure_callback_(CredentialsFailureStatus::ExpiredToken);
        } else {
          failure_callback_(CredentialsFailureStatus::Network);
        }
        // TODO: parse the error string. Example:
        /*
          <ErrorResponse xmlns="http://webservices.amazon.com/AWSFault/2005-15-09">
            <Error>
              <Type>Sender</Type>
              <Code>InvalidAction</Code>
              <Message>Could not find operation AssumeRoleWithWebIdentity for version NO_VERSION_SPECIFIED</Message>
            </Error>
            <RequestId>72168399-bcdd-4248-bf57-bf5d4a6dc07d</RequestId>
          </ErrorResponse>
        */
      } else {
        ENVOY_LOG(debug, "{}: assume role with token [uri = {}]: response status code {}", __func__,
                  uri_->uri(), status_code);
        ENVOY_LOG(trace, "{}: headers: {}", __func__, response->headers());
        failure_callback_(CredentialsFailureStatus::Network);
      }
    }
    reset();
  }

  void onFailure(const Http::AsyncClient::Request&,
                 Http::AsyncClient::FailureReason reason) override {
    ENVOY_LOG(debug, "{}: assume role with token [uri = {}]: network error {}", __func__, uri_->uri(),
              enumToInt(reason));
    complete_ = true;
    failure_callback_(CredentialsFailureStatus::Network);
    reset();
  }

  void onBeforeFinalizeUpstreamSpan(Tracing::Span&, const Http::ResponseHeaderMap*) override {}

private:
  Upstream::ClusterManager& cm_;
  Api::Api& api_;
  bool complete_{};
  SuccessCallback success_callback_;
  FailureCallback failure_callback_;
  const envoy::config::core::v3::HttpUri* uri_{};
  Http::AsyncClient::Request* request_{};

  void reset() {
    request_ = nullptr;
    success_callback_ = nullptr;
    failure_callback_ = nullptr;
    uri_ = nullptr;
  }
};
} // namespace


StsFetcherPtr StsFetcher::create(Upstream::ClusterManager& cm, Api::Api& api) {
  return std::make_unique<StsFetcherImpl>(cm, api);
}

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
