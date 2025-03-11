#include "source/common/buffer/buffer_impl.h"

#include "source/extensions/transformers/aws_lambda/api_gateway_transformer.h"

#include "test/mocks/http/mocks.h"

#include "nlohmann/json.hpp"

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

TEST(ApiGatewayTransformer, transform_response_ratelimited_lambda) {
  Http::TestResponseHeaderMapImpl response_headers{{":status", "429"}};
  Buffer::OwnedImpl body(R"json({
    "Reason": "TestReason",
    "Type": "User",
    "message": "Some message"
  })json");
  NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_{};

  ApiGatewayTransformer transformer;
  transformer.transform_response(&response_headers, body, filter_callbacks_);

  EXPECT_EQ("500", response_headers.getStatusValue());
  EXPECT_EQ("429", response_headers.get_(LAMBDA_STATUS_CODE_HEADER));
  EXPECT_EQ("TestReason", response_headers.get_(LAMBDA_STATUS_REASON_HEADER));
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

TEST(ApiGatewayTransformer, transform_non_string_headers) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {"x-test", "789"},
                                         {":path", "/users/123"}};
  Http::TestResponseHeaderMapImpl response_headers{};
  Buffer::OwnedImpl body("{"
      "\"statusCode\": 200,"
      "\"headers\": {"
          "\"string-header\": \"test_value\","
          "\"number-header\": 400,"
          "\"object-header\": {"
              "\"test\": \"test-value\""
          "}"
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

  auto lowercase_string_header_name = Http::LowerCaseString("string-header");
  auto header_values = response_headers.get(lowercase_string_header_name);
  EXPECT_EQ(header_values.empty(), false);
  auto str_header_value = header_values[0]->value().getStringView();
  EXPECT_EQ("test_value", str_header_value);

  auto lowercase_number_header_name = Http::LowerCaseString("number-header");
  auto number_header_values = response_headers.get(lowercase_number_header_name);
  EXPECT_EQ(number_header_values.empty(), false);
  auto number_header_value = number_header_values[0]->value().getStringView();
  EXPECT_EQ("400", number_header_value);

  auto lowercase_object_header_name = Http::LowerCaseString("object-header");
  auto object_header_values = response_headers.get(lowercase_object_header_name);
  EXPECT_EQ(object_header_values.empty(), false);
  auto object_header_value = object_header_values[0]->value().getStringView();
  EXPECT_EQ("{\"test\":\"test-value\"}", object_header_value);
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
  transformer.transform(request_headers, &headers, body, filter_callbacks_);

  // Nothing should be transformed in this case -- confirm that the headers and body are unchanged.
  EXPECT_EQ(headers, request_headers);
  EXPECT_EQ(body.toString(), "{}");
}

TEST(ApiGatewayTransformer, error) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {"x-test", "789"},
                                         {":path", "/users/123"}};
  Http::TestResponseHeaderMapImpl response_headers{};
  Buffer::OwnedImpl body("{invalid json}");
  ApiGatewayTransformer transformer;
  NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_{};
  transformer.transform(response_headers, &headers, body, filter_callbacks_);

  EXPECT_EQ(response_headers.getStatusValue(), "500");
  EXPECT_EQ(response_headers.get(Http::LowerCaseString("content-type"))[0]->value().getStringView(), "text/plain");
  EXPECT_EQ(response_headers.get(Http::LowerCaseString("x-amzn-errortype"))[0]->value().getStringView(), "500");
}

// helper used in multi value headers type safety tests
// - bodyPtr: json payload in the format used by AWS API Gateway/returned from an upstream Lambda
// - expected_error_message: if present, expect that an error message will be logged that contains this string
// - expected_multi_value_header: if present, expect that the first value of the 'test-multi-header' header in the response will be equal to this string,
void test_multi_value_headers_transformation(
    std::unique_ptr<Buffer::OwnedImpl> bodyPtr,
    std::string expected_error_message = "Error transforming response: [json.exception.type_error.302] type must be string, but is null",
    std::string expected_multi_value_header = "multi-value-0,multi-value-1,multi-value-2") {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {"x-test", "789"},
                                         {":path", "/users/123"}};
  Http::TestResponseHeaderMapImpl response_headers{};
  ApiGatewayTransformer transformer;
  NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_{};

  if (expected_error_message.empty()) {
    transformer.transform(response_headers, &headers, *bodyPtr, filter_callbacks_);
    auto lowercase_multi_header_name = Http::LowerCaseString("test-multi-header");
    auto header_values = response_headers.get(lowercase_multi_header_name);

    if (expected_multi_value_header.empty()) {
      EXPECT_EQ(header_values.empty(), true);
      return;
    }

    EXPECT_EQ(header_values.empty(), false);
    auto str_header_value = header_values[0]->value().getStringView();
    EXPECT_EQ(expected_multi_value_header, str_header_value);
  } else {
    EXPECT_LOG_CONTAINS(
      "debug",
      expected_error_message,
      transformer.transform(response_headers, &headers, *bodyPtr, filter_callbacks_)
    );
  }
}

// helper used in status code type safety tests
// - bodyPtr: json payload in the format used by AWS API Gateway/returned from an upstream Lambda
// - expected_error_message: if present, expect that an error message will be logged that contains this string
// - expected_status_code: if present, expect that the status code in the response headers will be equal to this string
void test_status_code_transform(
    std::unique_ptr<Buffer::OwnedImpl> bodyPtr,
    std::string expected_error_message = "Error transforming response: [json.exception.type_error.302] type must be number, but is",
    std::string expected_status_code = "200") {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {"x-test", "789"},
                                         {":path", "/users/123"}};
  Http::TestResponseHeaderMapImpl response_headers{};
  ApiGatewayTransformer transformer;
  NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_{};

  if (expected_error_message.empty()) {
    transformer.transform(response_headers, &headers, *bodyPtr, filter_callbacks_);
    EXPECT_EQ(expected_status_code, response_headers.getStatusValue());
  } else {
    EXPECT_LOG_CONTAINS(
      "debug",
      expected_error_message,
      transformer.transform(response_headers, &headers, *bodyPtr, filter_callbacks_)
    );
  }
}

///////////////////////////////////////////
// multi value headers type safety tests //
///////////////////////////////////////////
TEST(ApiGatewayTransformer, transform_null_multi_value_header) {
  test_multi_value_headers_transformation(
    std::make_unique<Buffer::OwnedImpl>(R"json({
      "multiValueHeaders": {
          "test-multi-header": null
      }
    })json"),
    "",
    ""
  );
}

TEST(ApiGatewayTransformer, transform_int_multi_value_header) {
  test_multi_value_headers_transformation(
    std::make_unique<Buffer::OwnedImpl>(R"json({
      "multiValueHeaders": {
          "test-multi-header": 123
      }
    })json"),
    "",
    "123"
  );
}

TEST(ApiGatewayTransformer, transform_float_multi_value_header) {
  test_multi_value_headers_transformation(
    std::make_unique<Buffer::OwnedImpl>(R"json({
      "multiValueHeaders": {
          "test-multi-header": 123.456
      }
    })json"),
    "",
    "123.456"
  );
}

TEST(ApiGatewayTransformer, transform_object_multi_value_header) {
  test_multi_value_headers_transformation(
    std::make_unique<Buffer::OwnedImpl>(R"json({
      "multiValueHeaders": {
          "test-multi-header": {"test": "test-value"}
      }
    })json"),
    "Returning error with message: invalid multi header value object",
    ""
    );
}

TEST(ApiGatewayTransformer, transform_list_multi_value_header) {
  test_multi_value_headers_transformation(
    std::make_unique<Buffer::OwnedImpl>(R"json({
      "multiValueHeaders": {
          "test-multi-header": ["test-value"]
      }
    })json"),
    "",
    "test-value"
    );
}

TEST(ApiGatewayTransformer, transform_list_with_null_multi_value_header) {
  test_multi_value_headers_transformation(
    std::make_unique<Buffer::OwnedImpl>(R"json({
      "multiValueHeaders": {
          "test-multi-header": ["test-value", null]
      }
    })json"),
    "",
    "test-value,null"
    );
}

TEST(ApiGatewayTransformer, transform_list_with_int_multi_value_header) {
  test_multi_value_headers_transformation(
    std::make_unique<Buffer::OwnedImpl>(R"json({
      "multiValueHeaders": {
          "test-multi-header": ["test-value", 123]
      }
    })json"),
    "",
    "test-value,123"
    );
}

TEST(ApiGatewayTransformer, transform_list_with_float_multi_value_header) {
  test_multi_value_headers_transformation(
    std::make_unique<Buffer::OwnedImpl>(R"json({
      "multiValueHeaders": {
          "test-multi-header": ["test-value", 123.456]
      }
    })json"),
    "",
    "test-value,123.456"
    );
}

TEST(ApiGatewayTransformer, transform_list_with_object_multi_value_header) {
  test_multi_value_headers_transformation(
    std::make_unique<Buffer::OwnedImpl>(R"json({
      "multiValueHeaders": {
          "test-multi-header": ["test-value", {"test": "test-value"}]
      }
    })json"),
    "",
    "test-value,{\"test\":\"test-value\"}"
    );
}

TEST(ApiGatewayTransformer, transform_list_with_list_multi_value_header) {
  test_multi_value_headers_transformation(
    std::make_unique<Buffer::OwnedImpl>(R"json({
      "multiValueHeaders": {
          "test-multi-header": ["test-value", ["test-value"]]
      }
    })json"),
    "",
    "test-value,[\"test-value\"]"
    );
}

TEST(ApiGatewayTransformer, transform_list_with_list_with_null_multi_value_header) {
  test_multi_value_headers_transformation(
    std::make_unique<Buffer::OwnedImpl>(R"json({
      "multiValueHeaders": {
          "test-multi-header": ["test-value", [null]]
      }
    })json"),
    "",
    "test-value,[null]"
    );
}

///////////////////////////////////
// status code type safety tests //
///////////////////////////////////
TEST(ApiGatewayTransformer, transform_null_status_code) {
  test_status_code_transform(
    std::make_unique<Buffer::OwnedImpl>(R"json({
      "statusCode": null
    })json"),
    "cannot parse non unsigned integer status code",
    ""
  );
}

TEST(ApiGatewayTransformer, transform_string_status_code) {
  test_status_code_transform(
    std::make_unique<Buffer::OwnedImpl>(R"json({
      "statusCode": "200"
    })json"),
    "cannot parse non unsigned integer status code",
    ""
  );
}

TEST(ApiGatewayTransformer, transform_string_non_int_status_code) {
  test_status_code_transform(
    std::make_unique<Buffer::OwnedImpl>(R"json({
      "statusCode": "200fasdfasdf"
    })json"),
    "cannot parse non unsigned integer status code",
    ""
  );
}

TEST(ApiGatewayTransformer, transform_int_status_code) {
  test_status_code_transform(
    std::make_unique<Buffer::OwnedImpl>(R"json({
      "statusCode": 200
    })json"),
    "",
    "200"
  );
}

TEST(ApiGatewayTransformer, transform_negative_int_status_code) {
  test_status_code_transform(
    std::make_unique<Buffer::OwnedImpl>(R"json({
      "statusCode": -200
    })json"),
    "cannot parse non unsigned integer status code",
    "" 
  );
}


// note to reviewers: as it stands, this is a breaking change
// we used to support float input for status code (which would be rounded down to the nearest integer)
// this PR propses that we reject float input for status code
TEST(ApiGatewayTransformer, transform_float_status_code) {
  test_status_code_transform(
    std::make_unique<Buffer::OwnedImpl>(R"json({
      "statusCode": 201.6
    })json"),
    "cannot parse non unsigned integer status code",
    "" 
  );
}

TEST(ApiGatewayTransformer, transform_object_status_code) {
  test_status_code_transform(
    std::make_unique<Buffer::OwnedImpl>(R"json({
      "statusCode": {"test": "test-value"}
    })json"),
    "cannot parse non unsigned integer status code",
    ""
  );
}

TEST(ApiGatewayTransformer, transform_list_status_code) {
  test_status_code_transform(
    std::make_unique<Buffer::OwnedImpl>(R"json({
      "statusCode": ["test-value"]
    })json"),
    "cannot parse non unsigned integer status code",
    ""
  );
}


} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
