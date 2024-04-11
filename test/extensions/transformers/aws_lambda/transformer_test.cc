#include "source/common/buffer/buffer_impl.h"

#include "source/extensions/transformers/aws_lambda/api_gateway_transformer.h"

#include "test/mocks/http/mocks.h"

#include "nlohmann/json.hpp"
// #include "gmock/gmock.h"
// #include "gtest/gtest.h"

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

// TEST(ApiGatewayTransformer, transform_null_multi_value_headers) {
//   Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
//                                          {":authority", "www.solo.io"},
//                                          {"x-test", "789"},
//                                          {":path", "/users/123"}};
//   Http::TestResponseHeaderMapImpl response_headers{};
//   Buffer::OwnedImpl body("{"
//       "\"multiValueHeaders\": {"
//           "\"test-multi-header\": [\"multi-value-1\", null]"
//       "}"
//   "}");

//   ApiGatewayTransformer transformer;
//   NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_{};
//   EXPECT_LOG_CONTAINS(
//     "debug",
//     "Error transforming response: [json.exception.type_error.302] type must be string, but is null",
//     transformer.transform(response_headers, &headers, body, filter_callbacks_)
//   );
// }

void test_transform_multi_value_headers(
    std::unique_ptr<Buffer::OwnedImpl> bodyPtr,
    std::string expected_error_message = "Error transforming response: [json.exception.type_error.302] type must be string, but is null",
    // expected response
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
      // "Error transforming response: [json.exception.type_error.302] type must be string, but is null",
      transformer.transform(response_headers, &headers, *bodyPtr, filter_callbacks_)
    );
  }
}

TEST(ApiGatewayTransformer, transform_null_multi_value_header) {
  test_transform_multi_value_headers(
    std::make_unique<Buffer::OwnedImpl>(R"json({
      "multiValueHeaders": {
          "test-multi-header": null
      }
    })json"),
    "",
    ""
  );
}

//// GOOD
TEST(ApiGatewayTransformer, transform_int_multi_value_header) {
  test_transform_multi_value_headers(
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
  test_transform_multi_value_headers(
    std::make_unique<Buffer::OwnedImpl>(R"json({
      "multiValueHeaders": {
          "test-multi-header": 123.456
      }
    })json"),
    "",
    "123.456"
  );
}

// This now gets rejected
TEST(ApiGatewayTransformer, transform_object_multi_value_header) {
  test_transform_multi_value_headers(
    std::make_unique<Buffer::OwnedImpl>(R"json({
      "multiValueHeaders": {
          "test-multi-header": {"test": "test-value"}
      }
    })json"),
    "Returning error with message: invalid multi header value object",
    ""
    );
}

// This is how it's supposed to work.... i would be disappointed if it didn't
TEST(ApiGatewayTransformer, transform_list_multi_value_header) {
  test_transform_multi_value_headers(
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
  test_transform_multi_value_headers(
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
  test_transform_multi_value_headers(
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
  test_transform_multi_value_headers(
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
  test_transform_multi_value_headers(
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
  test_transform_multi_value_headers(
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
  test_transform_multi_value_headers(
    std::make_unique<Buffer::OwnedImpl>(R"json({
      "multiValueHeaders": {
          "test-multi-header": ["test-value", [null]]
      }
    })json"),
    "",
    "test-value,[null]"
    );
}

void test_transform_status_code(
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

TEST(ApiGatewayTransformer, transform_null_status_code) {
  test_transform_status_code(
    std::make_unique<Buffer::OwnedImpl>(R"json({
      "statusCode": null
    })json"),
    "cannot parse non-integer status code",
    ""
  );
}

TEST(ApiGatewayTransformer, transform_string_status_code) {
  test_transform_status_code(
    std::make_unique<Buffer::OwnedImpl>(R"json({
      "statusCode": "200"
    })json"),
    "cannot parse non-integer status code",
    ""
  );
}

TEST(ApiGatewayTransformer, transform_string_non_int_status_code) {
  test_transform_status_code(
    std::make_unique<Buffer::OwnedImpl>(R"json({
      "statusCode": "200fasdfasdf"
    })json"),
    "cannot parse non-integer status code",
    ""
  );
}

TEST(ApiGatewayTransformer, transform_int_status_code) {
  test_transform_status_code(
    std::make_unique<Buffer::OwnedImpl>(R"json({
      "statusCode": 200
    })json"),
    "",
    "200"
  );
}

// as it stands, this is a breaking change
TEST(ApiGatewayTransformer, transform_float_status_code) {
  test_transform_status_code(
    std::make_unique<Buffer::OwnedImpl>(R"json({
      "statusCode": 201.6
    })json"),
    // "",
    // "200"
    "cannot parse non-integer status code",
    "" 
  );
}

TEST(ApiGatewayTransformer, transform_object_status_code) {
  test_transform_status_code(
    std::make_unique<Buffer::OwnedImpl>(R"json({
      "statusCode": {"test": "test-value"}
    })json"),
    // "Error parsing statusCode: [json.exception.type_error.302] type must be number, but is object",
    "cannot parse non-integer status code",
    ""
  );
}

TEST(ApiGatewayTransformer, transform_list_status_code) {
  test_transform_status_code(
    std::make_unique<Buffer::OwnedImpl>(R"json({
      "statusCode": ["test-value"]
    })json"),
    // "Error parsing statusCode: [json.exception.type_error.302] type must be number, but is array",
    "cannot parse non-integer status code",
    ""
  );
}

// TEST(ApiGatewayTransformer, transform_dangerous_status_code_values) {
//     Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
//                                          {":authority", "www.solo.io"},
//                                          {"x-test", "789"},
//                                          {":path", "/users/123"}};
//   Http::TestResponseHeaderMapImpl response_headers{};
//   std::map<std::string, std::unique_ptr<Buffer::OwnedImpl>> bodies = {};

//   bodies["string status code"] = std::make_unique<Buffer::OwnedImpl>(R"json({
//       "statusCode": "200"
//     })json");
//   bodies["null status code"] = std::make_unique<Buffer::OwnedImpl>(R"json({
//       "statusCode": null
//     })json");
//   bodies["object status code"] = std::make_unique<Buffer::OwnedImpl>(R"json({
//       "statusCode": {"test": "test-value"}
//     })json");
//   bodies["list status code"] = std::make_unique<Buffer::OwnedImpl>(R"json({
//       "statusCode": ["test-valuee"]
//     })json");

//   for (const auto& [bodyName, bodyPtr] : bodies) {
//     ApiGatewayTransformer transformer;
//     NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_{};
//     std::cout << "Processing body: " + bodyName << std::endl;
//     EXPECT_LOG_CONTAINS(
//       "debug",
//       "Error parsing statusCode: [json.exception.type_error.302] type must be number, but is",
//       transformer.transform(response_headers, &headers, *bodyPtr, filter_callbacks_)
//     );
//   }
// }

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

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy