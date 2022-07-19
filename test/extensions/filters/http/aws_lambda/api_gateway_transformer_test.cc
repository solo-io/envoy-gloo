#include "source/common/buffer/buffer_impl.h"

#include "source/extensions/filters/http/aws_lambda/api_gateway_transformer.h"

#include "test/mocks/http/mocks.h"
#include "test/mocks/server/mocks.h"
#include "test/test_common/utility.h"

#include "fmt/format.h"
#include "nlohmann/json.hpp"
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

using json = nlohmann::json;

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {

TEST(ApiGatewayTransformer, transform) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {"x-test", "789"},
                                         {":path", "/users/123"}};
  Http::TestResponseHeaderMapImpl response_headers{};
  Buffer::OwnedImpl body("{"
      "\"statusCode\": 200,"
      "\"headers\": {"
          "\"Content-Type\": \"application/json\""
      "}"
  "}");

  ApiGatewayTransformer transformer;
  NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_{};
  transformer.transform(response_headers, &headers, body, filter_callbacks_);

  std::string res = body.toString();
  json actual = json::parse(res);
  auto expected = R"(
  {}
)"_json;

  EXPECT_EQ(expected, actual);
  EXPECT_EQ("200", response_headers.getStatusValue());
  EXPECT_EQ("application/json", response_headers.getContentTypeValue());
}

TEST(ApiGatewayTransformer, transform_body) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {"x-test", "789"},
                                         {":path", "/users/123"}};
  Http::TestResponseHeaderMapImpl response_headers{};
  Buffer::OwnedImpl body("{"
      "\"statusCode\": 200,"
      "\"headers\": {"
          "\"Content-Type\": \"application/json\""
      "},"
      "\"body\": {"
          "\"test\": \"test-value\""
      "}"
  "}");

  ApiGatewayTransformer transformer;
  NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_{};
  transformer.transform(response_headers, &headers, body, filter_callbacks_);

  std::string res = body.toString();
  json actual = json::parse(res);
  auto expected = R"(
  {"test": "test-value"}
)"_json;

  EXPECT_EQ(expected, actual);
  EXPECT_EQ("200", response_headers.getStatusValue());
  EXPECT_EQ("application/json", response_headers.getContentTypeValue());
}

TEST(ApiGatewayTransformer, transform_multi_value_headers) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {"x-test", "789"},
                                         {":path", "/users/123"}};
  Http::TestResponseHeaderMapImpl response_headers{};
  Buffer::OwnedImpl body("{"
      "\"statusCode\": 200,"
      "\"headers\": {"
          "\"Content-Type\": \"application/json\""
      "},"
      "\"multiValueHeaders\": {"
          "\"test-multi-header\": [\"multi-value-1\", \"multi-value-2\"]"
      "},"
      "\"body\": {"
          "\"test\": \"test-value\""
      "}"
  "}");

  ApiGatewayTransformer transformer;
  NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_{};
  transformer.transform(response_headers, &headers, body, filter_callbacks_);

  std::string res = body.toString();
  json actual = json::parse(res);
  auto expected = R"(
  {"test": "test-value"}
)"_json;

  EXPECT_EQ(expected, actual);
  EXPECT_EQ("200", response_headers.getStatusValue());
  EXPECT_EQ("application/json", response_headers.getContentTypeValue());
  auto lowercase_multi_header_name = Http::LowerCaseString("test-multi-header");
  auto header_values = response_headers.get(lowercase_multi_header_name);
  EXPECT_EQ(header_values.empty(), false);
  auto str_header_value = header_values[0]->value().getStringView();
  EXPECT_EQ("multi-value-1,multi-value-2", str_header_value);
}

TEST(ApiGatewayTransformer, transform_single_and_multi_value_headers) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {"x-test", "789"},
                                         {":path", "/users/123"}};
  Http::TestResponseHeaderMapImpl response_headers{};
  Buffer::OwnedImpl body("{"
      "\"statusCode\": 200,"
      "\"headers\": {"
          "\"test-multi-header\": \"multi-value-0\""
      "},"
      "\"multiValueHeaders\": {"
          "\"test-multi-header\": [\"multi-value-1\", \"multi-value-2\"]"
      "},"
      "\"body\": {"
          "\"test\": \"test-value\""
      "}"
  "}");

  ApiGatewayTransformer transformer;
  NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_{};
  transformer.transform(response_headers, &headers, body, filter_callbacks_);

  std::string res = body.toString();
  json actual = json::parse(res);
  auto expected = R"(
  {"test": "test-value"}
)"_json;

  EXPECT_EQ(expected, actual);
  EXPECT_EQ("200", response_headers.getStatusValue());
  auto lowercase_multi_header_name = Http::LowerCaseString("test-multi-header");
  auto header_values = response_headers.get(lowercase_multi_header_name);
  EXPECT_EQ(header_values.empty(), false);
  auto str_header_value = header_values[0]->value().getStringView();
  EXPECT_EQ("multi-value-0,multi-value-1,multi-value-2", str_header_value);
}

TEST(ApiGatewayTransformer, base64decode) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {"x-test", "789"},
                                         {":path", "/users/123"}};
  Http::TestResponseHeaderMapImpl response_headers{};
  Buffer::OwnedImpl body("{ \"isBase64Encoded\": true, \"statusCode\": 201,"
            "\"body\": \"SGVsbG8gZnJvbSBMYW1iZGEgKG9wdGlvbmFsKQ==\"}");

  ApiGatewayTransformer transformer;
  NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_{};
  transformer.transform(response_headers, &headers, body, filter_callbacks_);

  std::string res = body.toString();

  EXPECT_EQ("Hello from Lambda (optional)", res);
  EXPECT_EQ("201", response_headers.getStatusValue());
}

TEST(ApiGatewayTransformer, multiple_single_headers) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {"x-test", "789"},
                                         {":path", "/users/123"}};
  Http::TestResponseHeaderMapImpl response_headers{};
  Buffer::OwnedImpl body("{"
      "\"statusCode\": 200,"
      "\"headers\": {"
          "\"test-multi-header\": \"multi-value-0\","
          "\"test-multi-header\": \"multi-value-1\""
      "},"
      "\"body\": {"
          "\"test\": \"test-value\""
      "}"
  "}");

  ApiGatewayTransformer transformer;
  NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_{};
  transformer.transform(response_headers, &headers, body, filter_callbacks_);

  std::string res = body.toString();
  json actual = json::parse(res);
  auto expected = R"(
  {"test": "test-value"}
)"_json;

  EXPECT_EQ(expected, actual);
  EXPECT_EQ("200", response_headers.getStatusValue());
  auto lowercase_multi_header_name = Http::LowerCaseString("test-multi-header");
  auto header_values = response_headers.get(lowercase_multi_header_name);
  EXPECT_EQ(header_values.empty(), false);
  auto str_header_value = header_values[0]->value().getStringView();
  EXPECT_EQ("multi-value-1", str_header_value);
}

TEST(ApiGatewayTransformer, request_path) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {"x-test", "789"},
                                         {":path", "/users/123"}};
  Http::TestRequestHeaderMapImpl request_headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {"x-test", "789"},
                                         {":path", "/users/123"}};
  Buffer::OwnedImpl body("{}");
  ApiGatewayTransformer transformer;
  NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_{};
  EXPECT_ANY_THROW(transformer.transform(request_headers, &headers, body, filter_callbacks_));
}

TEST(ApiGatewayTransformer, error) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {"x-test", "789"},
                                         {":path", "/users/123"}};
  Http::TestRequestHeaderMapImpl request_headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {"x-test", "789"},
                                         {":path", "/users/123"}};
  Buffer::OwnedImpl body("{invalid json}");
  ApiGatewayTransformer transformer;
  NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_{};
  EXPECT_ANY_THROW(transformer.transform(request_headers, &headers, body, filter_callbacks_));

  EXPECT_EQ(response_headers.getStatusValue(), "500");
  EXPECT_EQ(response_headers.get(Http::LowerCaseString("content-type")), "text/plain");
  EXPECT_EQ(response_headers.get(Http::LowerCaseString("x-amzn-errortype")), "500");
}

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
