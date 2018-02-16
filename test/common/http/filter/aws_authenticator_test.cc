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
};

TEST_F(AwsAuthenticatorTest, BodyHash) {
  std::string secretkey = "secretkey";
  std::string accesskey = "accesskey";
  AwsAuthenticator aws;
  aws.init(&accesskey, &secretkey);

  updatePayloadHash(aws, "\"abc\"");
  std::string hexsha = getBodyHexSha(aws);
  EXPECT_EQ("6cc43f858fbb763301637b5af970e2a46b46f461f27e5a0f41e009c59b827b25", hexsha);
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

} // namespace Http
} // namespace Envoy
