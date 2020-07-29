#include "extensions/filters/http/aws_lambda/sts_fetcher.h"

#include "envoy/config/core/v3/http_uri.pb.h"

#include "common/common/enum_to_int.h"
#include "common/common/regex.h"
#include "common/http/headers.h"
#include "common/http/utility.h"


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
              const std::string& role_arn,
              const std::string& web_token,
              StsFetcher::StsReceiver& receiver) override {
    ENVOY_LOG(trace, "{}", __func__);
    ASSERT(!receiver_);

    complete_ = false;
    receiver_ = &receiver;
    uri_ = &uri;

    // Check if cluster is configured, fail the request if not.
    // Otherwise cm_.httpAsyncClientForCluster will throw exception.
    if (cm_.get(uri.cluster()) == nullptr) {
      ENVOY_LOG(error, "{}: assume role with token [uri = {}] failed: [cluster = {}] is not configured",
                __func__, uri.uri(), uri.cluster());
      complete_ = true;
      receiver_->onStsError(StsFetcher::StsReceiver::Failure::Network);
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
        if (!matched_access_key.size() > 1) {
          receiver_->onStsError(StsFetcher::StsReceiver::Failure::InvalidSts);
        }

        std::smatch matched_secret_key;
        std::regex_search(body, matched_secret_key, secret_key_regex);
        if (!matched_secret_key.size() > 1) {
          receiver_->onStsError(StsFetcher::StsReceiver::Failure::InvalidSts);
        }
        std::smatch matched_session_token;
        std::regex_search(body, matched_session_token, session_token_regex);
        if (!matched_session_token.size() > 1) {
          receiver_->onStsError(StsFetcher::StsReceiver::Failure::InvalidSts);
        }

        Envoy::Extensions::Common::Aws::Credentials result(matched_access_key[1].str(), matched_secret_key[1].str(), matched_session_token[1].str());
        receiver_->onStsSuccess(std::move(result), SystemTime());
        // auto jwks =
        //     google::jwt_verify::Sts::createFrom(body, google::jwt_verify::Sts::Type::JWKS);
        // if (jwks->getStatus() == google::jwt_verify::Status::Ok) {
        //   ENVOY_LOG(debug, "{}: assume role with token [uri = {}]: succeeded", __func__, uri_->uri());
        //   receiver_->onStsSuccess(std::move(jwks));
        // } else {
        //   ENVOY_LOG(debug, "{}: assume role with token [uri = {}]: invalid jwks", __func__, uri_->uri());
        //   receiver_->onStsError(StsFetcher::StsReceiver::Failure::InvalidSts);
        // }
      } else {
        ENVOY_LOG(debug, "{}: assume role with token [uri = {}]: body is empty", __func__, uri_->uri());
        receiver_->onStsError(StsFetcher::StsReceiver::Failure::Network);
      }
    } else {
      ENVOY_LOG(debug, "{}: assume role with token [uri = {}]: response status code {}", __func__,
                uri_->uri(), status_code);
      receiver_->onStsError(StsFetcher::StsReceiver::Failure::Network);
    }
    reset();
  }

  void onFailure(const Http::AsyncClient::Request&,
                 Http::AsyncClient::FailureReason reason) override {
    ENVOY_LOG(debug, "{}: assume role with token [uri = {}]: network error {}", __func__, uri_->uri(),
              enumToInt(reason));
    complete_ = true;
    receiver_->onStsError(StsFetcher::StsReceiver::Failure::Network);
    reset();
  }

  void onBeforeFinalizeUpstreamSpan(Tracing::Span&, const Http::ResponseHeaderMap*) override {}

private:
  Upstream::ClusterManager& cm_;
  Api::Api& api_;
  bool complete_{};
  StsFetcher::StsReceiver* receiver_{};
  const envoy::config::core::v3::HttpUri* uri_{};
  Http::AsyncClient::Request* request_{};

  void reset() {
    request_ = nullptr;
    receiver_ = nullptr;
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
