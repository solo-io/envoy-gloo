#include "extensions/filters/http/aws_lambda/sts_fetcher.h"

#include "common/common/enum_to_int.h"
#include "common/common/regex.h"
#include "common/http/headers.h"
#include "common/http/utility.h"


namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {
namespace {

/*
  * AssumeRoleWithIdentity returns a set of temporary credentials with a minimum lifespan of 15 minutes.
  * https://docs.aws.amazon.com/STS/latest/APIReference/API_AssumeRoleWithWebIdentity.html
  * 
  * In order to ensure that credentials never expire, we default to slightly less than half of the lifecycle.
  * 
*/
constexpr std::chrono::milliseconds REFRESH_STS_CREDS =
    std::chrono::minutes(7);

constexpr char EXPIRATION_FORMAT[] = "%E4Y%m%dT%H%M%S%z";

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
              const std::string& role_arn,
              const std::string& web_token,
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
      failure_callback_(StsFetcher::Failure::Network);
      reset();
      return;
    }

    Http::RequestMessagePtr message = Http::Utility::prepareHeaders(uri);
    message->headers().setReferenceMethod(Http::Headers::get().MethodValues.Post);
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(api_.timeSource().systemTime().time_since_epoch()).count();
    const absl::string_view body = fmt::format("Action=AssumeRoleWithWebIdentity&RoleArn={}&RoleSessionName={}&WebIdentityToken={}", role_arn, now, web_token);
    message->body()->add(body);
    ENVOY_LOG(debug, "assume role with token from [uri = {}]: start", uri_->uri());
    auto options = Http::AsyncClient::RequestOptions()
                       .setTimeout(std::chrono::milliseconds(
                           DurationUtil::durationToMilliseconds(uri.timeout())));
    request_ =
        cm_.httpAsyncClientForCluster(uri.cluster()).send(std::move(message), *this, options);
  }

  // HTTP async receive methods
  void onSuccess(const Http::AsyncClient::Request&, Http::ResponseMessagePtr&& response) override {
    ENVOY_LOG(trace, "{}", __func__);
    complete_ = true;
    const uint64_t status_code = Http::Utility::getResponseStatus(response->headers());
    if (status_code == enumToInt(Http::Code::OK)) {
      ENVOY_LOG(debug, "{}: assume role with token [uri = {}]: success", __func__, uri_->uri());
      if (response->body()) {
        const auto len = response->body()->length();
        const auto body = std::string(static_cast<char*>(response->body()->linearize(len)), len);
        const auto access_key_regex = Regex::Utility::parseStdRegex("<AccessKeyId>.*?<\\/AccessKeyId>");
        const auto secret_key_regex = Regex::Utility::parseStdRegex("<SecretAccessKey>.*?<\\/SecretAccessKey>");
        const auto session_token_regex = Regex::Utility::parseStdRegex("<SessionToken>.*?<\\/SessionToken>");
        const auto expiration_regex = Regex::Utility::parseStdRegex("<Expiration>.*?<\\/Expiration>");

        std::smatch matched_access_key;
        std::regex_search(body, matched_access_key, access_key_regex);
        if (!(matched_access_key.size() > 1)) {
          ENVOY_LOG(trace, "response body did not contain access_key");
          failure_callback_(StsFetcher::Failure::InvalidSts);
        }

        std::smatch matched_secret_key;
        std::regex_search(body, matched_secret_key, secret_key_regex);
        if (!(matched_secret_key.size() > 1)) {
          ENVOY_LOG(trace, "response body did not contain secret_key");
          failure_callback_(StsFetcher::Failure::InvalidSts);
        }
        
        std::smatch matched_session_token;
        std::regex_search(body, matched_session_token, session_token_regex);
        if (!(matched_session_token.size() > 1)) {
          ENVOY_LOG(trace, "response body did not contain session_token");
          failure_callback_(StsFetcher::Failure::InvalidSts);
        }
        
        std::smatch matched_expiration;
        std::regex_search(body, matched_expiration, expiration_regex);
        if (!(matched_expiration.size() > 1)) {
          ENVOY_LOG(trace, "response body did not contain expiration");
          failure_callback_(StsFetcher::Failure::InvalidSts);
        }

        SystemTime expiration_time;
        absl::Time absl_expiration_time;
        if (absl::ParseTime(EXPIRATION_FORMAT, matched_expiration[1].str(), &absl_expiration_time, nullptr)) {
          ENVOY_LOG(trace, "Determined expiration time from STS credentials result");
          expiration_time = absl::ToChronoTime(absl_expiration_time);
        } else {
          expiration_time = api_.timeSource().systemTime() + REFRESH_STS_CREDS;
          ENVOY_LOG(trace, "Unable to determine expiration time from STS credentials result, using default");
        }

        StsCredentialsConstSharedPtr result = std::make_shared<const StsCredentials>(matched_access_key[1].str(), matched_secret_key[1].str(), matched_session_token[1].str(), expiration_time);
        success_callback_(result);

      } else {
        ENVOY_LOG(debug, "{}: assume role with token [uri = {}]: body is empty", __func__, uri_->uri());
        failure_callback_(StsFetcher::Failure::Network);
      }
    } else {
      ENVOY_LOG(debug, "{}: assume role with token [uri = {}]: response status code {}", __func__,
                uri_->uri(), status_code);
      failure_callback_(StsFetcher::Failure::Network);
    }
    reset();
  }

  void onFailure(const Http::AsyncClient::Request&,
                 Http::AsyncClient::FailureReason reason) override {
    ENVOY_LOG(debug, "{}: assume role with token [uri = {}]: network error {}", __func__, uri_->uri(),
              enumToInt(reason));
    complete_ = true;
    failure_callback_(StsFetcher::Failure::Network);
    reset();
  }

  void onBeforeFinalizeUpstreamSpan(Tracing::Span&, const Http::ResponseHeaderMap*) override {}

private:
  Upstream::ClusterManager& cm_;
  Api::Api& api_;
  bool complete_{};
  StsFetcher::SuccessCallback success_callback_;
  StsFetcher::FailureCallback failure_callback_;
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
