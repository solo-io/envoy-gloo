#pragma once

// #include "source/common/common/regex.h"
#include "source/common/regex/regex.h"
#include "source/common/singleton/const_singleton.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {

namespace {
/*
 * AssumeRoleWithIdentity returns a set of temporary credentials with a minimum
 * lifespan of 15 minutes.
 * https://docs.aws.amazon.com/STS/latest/APIReference/API_AssumeRoleWithWebIdentity.html
 *
 * In order to ensure that credentials never expire, we default to 2/3.
 *
 * This in combination with the very generous grace period which makes sure the
 * tokens are refreshed if they have < 5 minutes left on their lifetime. Whether
 * that lifetime is our prescribed, or from the response itself.
 */
constexpr std::chrono::milliseconds DUPE_REFRESH_STS_CREDS =
    std::chrono::minutes(10);

} // namespace


class StsResponseRegexValues {
public:
  StsResponseRegexValues() {

    // Initialize regex strings, should never fail
    regex_access_key =
        Solo::Regex::Utility::parseStdRegex("<AccessKeyId>(.*?)</AccessKeyId>");
    regex_secret_key = Solo::Regex::Utility::parseStdRegex(
        "<SecretAccessKey>(.*?)</SecretAccessKey>");
    regex_session_token =
        Solo::Regex::Utility::parseStdRegex("<SessionToken>(.*?)</SessionToken>");
    regex_expiration =
        Solo::Regex::Utility::parseStdRegex("<Expiration>(.*?)</Expiration>");
  };

  std::regex regex_access_key;

  std::regex regex_secret_key;

  std::regex regex_session_token;

  std::regex regex_expiration;
};

using StsResponseRegex = ConstSingleton<StsResponseRegexValues>;


} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy


