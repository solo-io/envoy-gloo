
#include <map>
#include <string>

#include "curl/curl.h"
#include "common/common/regex.h"
#include "envoy/stats/scope.h"
#include "envoy/stats/stats_macros.h"
#include "envoy/upstream/cluster_manager.h"
#include "extensions/filters/http/aws_lambda/sts_credentials_provider.h"


#include "absl/types/optional.h"
#include "api/envoy/config/filter/http/aws_lambda/v2/aws_lambda.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {





AwsLambdaFilterStats
StsCredentialsProvider::generateStats(Stats::Scope &scope) {
  return {ALL_AWS_LAMBDA_FILTER_STATS(POOL_COUNTER_PREFIX(scope, StatsConstants::get().Prefix),
                                      POOL_GAUGE_PREFIX(scope, StatsConstants::get().Prefix))};
}

const Envoy::Extensions::Common::Aws::Credentials StsCredentialsProvider::getCredentials(const std::string* role_arn_arg) {
  // check if credentials need refreshing
  // const auto now = api_.timeSource().systemTime();
  const auto token_file  = absl::NullSafeStringView(std::getenv(AWS_WEB_IDENTITY_TOKEN_FILE));
  ASSERT(!token_file.empty());

  const auto web_token = api_.fileSystem().fileReadToEnd(std::string(token_file));

  // Set ARN to env var
  std::string role_arn{}; 
  // If role_arn_ is present in protocol options, use that
  if (role_arn_arg != nullptr) {
    role_arn = *role_arn_arg;
  }  else {
    role_arn = absl::NullSafeStringView(std::getenv(AWS_ROLE_ARN));
  }
  // role_arn cannot be empty
  // ASSERT(role_arn != nullptr);
  
  const auto token_body = fetchCredentials(web_token, role_arn);

  if (token_body.has_value()) {
    ENVOY_LOG(error, "Could not fetch credentials via STS");
    return Envoy::Extensions::Common::Aws::Credentials();
  }

  const auto access_key_regex_ = Regex::Utility::parseStdRegex("<AccessKeyId>.*?<\\/AccessKeyId>");
  const auto secret_key_regex_ = Regex::Utility::parseStdRegex("<SecretAccessKey>.*?<\\/SecretAccessKey>");
  const auto session_token_regex_ = Regex::Utility::parseStdRegex("<SessionToken>.*?<\\/SessionToken>");
  const auto expiration_regex_ = Regex::Utility::parseStdRegex("<Expiration>.*?<\\/Expiration>");
  
  std::smatch matched;
  std::regex_search(token_body.value(), matched, access_key_regex_);
  if (matched.size() < 1) {
    return Envoy::Extensions::Common::Aws::Credentials();
  }

  return Envoy::Extensions::Common::Aws::Credentials();
};

static size_t curlCallback(char* ptr, size_t, size_t nmemb, void* data) {
  auto buf = static_cast<std::string*>(data);
  buf->append(ptr, nmemb);
  return nmemb;
}

absl::optional<std::string> StsCredentialsProvider::fetchCredentials(
    const std::string& jwt, const std::string& arn)  {
        static const size_t MAX_RETRIES = 4;
  static const std::chrono::milliseconds RETRY_DELAY{1000};
  static const std::chrono::seconds TIMEOUT{5};

  CURL* const curl = curl_easy_init();
  if (!curl) {
    return absl::nullopt;
  };

  curl_easy_setopt(curl, CURLOPT_URL, StsConstants::get().GlobalEndpoint.c_str());
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, TIMEOUT.count());
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 1);

  const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(api_.timeSource().systemTime().time_since_epoch()).count();
  const std::string post_params = fmt::format("Action=AssumeRoleWithWebIdentity&RoleArn={}&RoleSessionName={}&WebIdentityToken={}", arn, now, jwt);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_params);

  std::string buffer;
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlCallback);
  char errbuf[CURL_ERROR_SIZE];
  curl_easy_setopt (curl, CURLOPT_ERRORBUFFER, errbuf);
  for (size_t retry = 0; retry < MAX_RETRIES; retry++) {
    const CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
      break;
    }
    // ENVOY_LOG_MISC(debug, "Could not AssumeRoleWithWebIdentity: {}", curl_easy_strerror(res));
    buffer.clear();
    std::this_thread::sleep_for(RETRY_DELAY);
  }

  curl_easy_cleanup(curl);

  return buffer.empty() ? absl::nullopt : absl::optional<std::string>(buffer);
}

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
