#include <ctime>

#include "common/http/filter/aws_authenticator.h"

#include "test/mocks/common.h"
#include "test/mocks/server/mocks.h"
#include "test/mocks/upstream/mocks.h"
#include "test/test_common/utility.h"

#include "fmt/format.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::AtLeast;
using testing::Invoke;
using testing::Return;
using testing::ReturnPointee;
using testing::ReturnRef;
using testing::SaveArg;
using testing::WithArg;
using testing::_;

namespace Envoy {
namespace Http {

class AwsAuthenticatorTest : public testing::Test {

public:
  void updatePayloadHash(AwsAuthenticator &aws, std::string bodys) {
    Buffer::OwnedImpl body(bodys);
    aws.updatePayloadHash(body);
  }

  std::string getBodyHexSha(AwsAuthenticator &aws) {

    return aws.getBodyHexSha();
  }

  std::string get_query(const AwsAuthenticator &aws) {
    return std::string(aws.query_string_start_, aws.query_string_len_);
  }
  std::string get_url(const AwsAuthenticator &aws) {
    return std::string(aws.url_start_, aws.url_len_);
  }

  void set_guide_test_params(AwsAuthenticator &aws) {
    aws.service_ = &SERVICE;
    aws.method_ = &Envoy::Http::Headers::get().MethodValues.Get;
  }

  std::string
  signWithTime(AwsAuthenticator &aws, Envoy::Http::HeaderMap *request_headers,
               std::list<Envoy::Http::LowerCaseString> &&headers,
               const std::string &region,
               std::chrono::time_point<std::chrono::system_clock> now) {
    return aws.signWithTime(request_headers, std::move(headers), region, now);
  }

  static const std::string SERVICE;
};
const std::string AwsAuthenticatorTest::SERVICE = "service";

TEST_F(AwsAuthenticatorTest, BodyHash) {
  std::string secretkey = "secretkey";
  std::string accesskey = "accesskey";
  AwsAuthenticator aws;
  aws.init(&accesskey, &secretkey);

  updatePayloadHash(aws, "\"abc\"");
  std::string hexsha = getBodyHexSha(aws);
  EXPECT_EQ("6cc43f858fbb763301637b5af970e2a46b46f461f27e5a0f41e009c59b827b25",
            hexsha);
}

TEST_F(AwsAuthenticatorTest, UrlQuery) {
  std::string secretkey = "secretkey";
  std::string accesskey = "accesskey";

  std::string url = "/this-us-a-url-with-no-query";
  std::string query = "q=query";
  Envoy::Http::TestHeaderMapImpl headers;
  headers.insertPath().value(url + "?" + query);
  headers.insertMethod().value(std::string("GET"));
  headers.insertHost().value(std::string("www.solo.io"));

  AwsAuthenticator aws;
  aws.init(&accesskey, &secretkey);
  updatePayloadHash(aws, "abc");

  std::list<LowerCaseString> headers_to_sign;
  headers_to_sign.push_back(LowerCaseString("path"));
  aws.sign(&headers, std::move(headers_to_sign), "us-east-1");

  EXPECT_EQ(query, get_query(aws));
  EXPECT_EQ(url, get_url(aws));
}
// https://docs.aws.amazon.com/general/latest/gr/signature-v4-test-suite.html
TEST_F(AwsAuthenticatorTest, TestGuide) {
  std::string secretkey = "wJalrXUtnFEMI/K7MDENG+bPxRfiCYEXAMPLEKEY";
  std::string accesskey = "AKIDEXAMPLE";

  std::string url = "/?Param1=value1&Param2=value2";
  Envoy::Http::TestHeaderMapImpl headers;
  headers.insertPath().value(url);
  headers.insertMethod().value(std::string("GET"));
  headers.insertHost().value(std::string("example.amazonaws.com"));

  AwsAuthenticator aws;
  aws.init(&accesskey, &secretkey);

  set_guide_test_params(aws);

  struct tm timeinfo = {};
  timeinfo.tm_year = 2015 - 1900;
  timeinfo.tm_mon = 7; // 0 based august.
  timeinfo.tm_mday = 30;
  timeinfo.tm_hour = 12;
  timeinfo.tm_min = 36;
  timeinfo.tm_sec = 0;

  auto timet = std::mktime(&timeinfo);
  std::chrono::time_point<std::chrono::system_clock> awstime =
      std::chrono::system_clock::from_time_t(timet);
  ASSERT_EQ(Envoy::DateFormatter("%Y%m%dT%H%M%SZ").fromTime(awstime),
            "20150830T123600Z");

  std::string sig;
  {
    std::list<LowerCaseString> headers_to_sign;
    headers_to_sign.push_back(LowerCaseString("host"));
    sig = signWithTime(aws, &headers, std::move(headers_to_sign), "us-east-1",
                       awstime);
  }
  std::string expected = "AWS4-HMAC-SHA256 "
                         "Credential=AKIDEXAMPLE/20150830/us-east-1/service/"
                         "aws4_request, SignedHeaders=host;x-amz-date, "
                         "Signature="
                         "b97d918cfa904a5beff61c982a1b6f458b799221646efd99d3219"
                         "ec94cdf2500";

  EXPECT_EQ(expected, sig);
}

} // namespace Http
} // namespace Envoy
