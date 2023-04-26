#include <ctime>

#include "source/extensions/filters/http/aws_lambda/aws_authenticator.h"

#include "test/mocks/common.h"
#include "test/mocks/server/mocks.h"
#include "test/mocks/upstream/mocks.h"
#include "test/test_common/utility.h"

#include "fmt/format.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::_;
using testing::AtLeast;
using testing::Invoke;
using testing::Return;
using testing::ReturnPointee;
using testing::ReturnRef;
using testing::SaveArg;
using testing::WithArg;

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {

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
    return std::string(aws.query_string_);
  }
  std::string get_url(const AwsAuthenticator &aws) {
    return std::string(aws.url_base_);
  }

  void set_guide_test_params(AwsAuthenticator &aws) {
    aws.service_ = &SERVICE;
    aws.method_ = &Http::Headers::get().MethodValues.Get;
  }

  std::string
  signWithTime(AwsAuthenticator &aws, Http::RequestHeaderMap *request_headers,
               const HeaderList &headers, const std::string &region,
               std::chrono::time_point<std::chrono::system_clock> now) {
    return aws.signWithTime(request_headers, std::move(headers), region, now);
  }

  static const std::string SERVICE;
};
const std::string AwsAuthenticatorTest::SERVICE = "service";

TEST_F(AwsAuthenticatorTest, RepeatedlyBodyHash) {
  DangerousDeprecatedTestTime time;
  AwsAuthenticator aws(time.timeSystem());

  std::string secretkey = "secretkey";
  std::string accesskey = "accesskey";
  aws.init(&accesskey, &secretkey, nullptr);

  updatePayloadHash(aws, "\"abc\"");
  std::string hexsha = getBodyHexSha(aws);
  EXPECT_EQ("6cc43f858fbb763301637b5af970e2a46b46f461f27e5a0f41e009c59b827b25",
            hexsha);
}


TEST_F(AwsAuthenticatorTest, UrlQuery) {
  DangerousDeprecatedTestTime time;
  AwsAuthenticator aws(time.timeSystem());

  std::string secretkey = "secretkey";
  std::string accesskey = "accesskey";
  aws.init(&accesskey, &secretkey, nullptr);

  std::string url = "/this-us-a-url-with-no-query";
  std::string query = "q=query";
  Http::TestRequestHeaderMapImpl headers;
  headers.setPath(url + "?" + query);
  headers.setMethod(std::string("GET"));
  headers.setHost(std::string("www.solo.io"));

  updatePayloadHash(aws, "abc");

  HeaderList headers_to_sign =
      AwsAuthenticator::createHeaderToSign({Http::LowerCaseString("path")});
  aws.sign(&headers, headers_to_sign, "us-east-1");

  EXPECT_EQ(query, get_query(aws));
  EXPECT_EQ(url, get_url(aws));
}
// https://docs.aws.amazon.com/general/latest/gr/signature-v4-test-suite.html
TEST_F(AwsAuthenticatorTest, TestGuide) {
  DangerousDeprecatedTestTime time;
  AwsAuthenticator aws(time.timeSystem());

  std::string secretkey = "wJalrXUtnFEMI/K7MDENG+bPxRfiCYEXAMPLEKEY";
  std::string accesskey = "AKIDEXAMPLE";
  std::string sessiontoken = "session_token";
  aws.init(&accesskey, &secretkey, &sessiontoken);

  std::string url = "/?Param1=value1&Param2=value2";
  Http::TestRequestHeaderMapImpl headers;
  headers.setPath(url);
  headers.setMethod(std::string("GET"));
  headers.setHost(std::string("example.amazonaws.com"));

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
  ASSERT_EQ(DateFormatter("%Y%m%dT%H%M%SZ").fromTime(awstime),
            "20150830T123600Z");

  std::string sig;
  {

    HeaderList headers_to_sign =
        AwsAuthenticator::createHeaderToSign({Http::LowerCaseString("host")});
    sig = signWithTime(aws, &headers, headers_to_sign, "us-east-1", awstime);
  }
  std::string expected = "AWS4-HMAC-SHA256 "
                         "Credential=AKIDEXAMPLE/20150830/us-east-1/service/"
                         "aws4_request, SignedHeaders=host;x-amz-date, "
                         "Signature="
                         "b97d918cfa904a5beff61c982a1b6f458b799221646efd99d3219"
                         "ec94cdf2500";

  EXPECT_EQ(expected, sig);

  // CHeck that the session_token header is set correctly
  auto session_header =
      headers.get(AwsAuthenticatorConsts::get().SecurityTokenHeader)[0]
          ->value()
          .getStringView();
  EXPECT_EQ(session_header, sessiontoken);
}

TEST_F(AwsAuthenticatorTest, UrlEncoding) {
  DangerousDeprecatedTestTime time;
  AwsAuthenticator aws(time.timeSystem());

  std::string secretkey = "secretkey";
  std::string accesskey = "accesskey";
  aws.init(&accesskey, &secretkey, nullptr);

  // example url representing a lambda addressed by ARN
  std::string url = "/2015-03-31/functions/arn%3Aaws%3Alambda%3Aus-east-1%3A"
                    "123456789012%3Afunction%3Asome-function/invocations";
  Http::TestRequestHeaderMapImpl headers;
  headers.setPath(url);
  headers.setMethod(std::string("GET"));
  headers.setHost(std::string("www.solo.io"));

  updatePayloadHash(aws, "abc");

  HeaderList headers_to_sign =
      AwsAuthenticator::createHeaderToSign({Http::LowerCaseString("path")});
  aws.sign(&headers, headers_to_sign, "us-east-1");

  // the URL needs to be _double_ encoded, due to a bug in AWS
  std::string encoded_url = "/2015-03-31/functions/arn%253Aaws%253Alambda%253A"
                            "us-east-1%253A123456789012%253Afunction%253A"
                            "some-function/invocations";
  EXPECT_EQ(encoded_url, get_url(aws));
}

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
