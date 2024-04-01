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

// TEST(ApiGatewayTransformer, transform) {
//   Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
//                                          {":authority", "www.solo.io"},
//                                          {"x-test", "789"},
//                                          {":path", "/users/123"}};
//   Http::TestResponseHeaderMapImpl response_headers{};
//   Buffer::OwnedImpl body("{"
//       "\"statusCode\": 200,"
//       "\"headers\": {"
//           "\"Content-Type\": \"application/json\""
//       "}"
//   "}");

//   ApiGatewayTransformer transformer;
//   NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_{};
//   transformer.transform(response_headers, &headers, body, filter_callbacks_);

//   std::string res = body.toString();
//   json actual = json::parse(res);
//   auto expected = R"(
//   {}
// )"_json;

//   EXPECT_EQ(expected, actual);
//   EXPECT_EQ("200", response_headers.getStatusValue());
//   EXPECT_EQ("application/json", response_headers.getContentTypeValue());
// }

// TEST(ApiGatewayTransformer, transform_body) {
//   Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
//                                          {":authority", "www.solo.io"},
//                                          {"x-test", "789"},
//                                          {":path", "/users/123"}};
//   Http::TestResponseHeaderMapImpl response_headers{};
//   Buffer::OwnedImpl body("{"
//       "\"statusCode\": 200,"
//       "\"headers\": {"
//           "\"Content-Type\": \"application/json\""
//       "},"
//       "\"body\": {"
//           "\"test\": \"test-value\""
//       "}"
//   "}");

//   ApiGatewayTransformer transformer;
//   NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_{};
//   transformer.transform(response_headers, &headers, body, filter_callbacks_);

//   std::string res = body.toString();
//   json actual = json::parse(res);
//   auto expected = R"(
//   {"test": "test-value"}
// )"_json;

//   EXPECT_EQ(expected, actual);
//   EXPECT_EQ("200", response_headers.getStatusValue());
//   EXPECT_EQ("application/json", response_headers.getContentTypeValue());
// }

// TEST(ApiGatewayTransformer, transform_multi_value_headers) {
//   Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
//                                          {":authority", "www.solo.io"},
//                                          {"x-test", "789"},
//                                          {":path", "/users/123"}};
//   Http::TestResponseHeaderMapImpl response_headers{};
//   Buffer::OwnedImpl body("{"
//       "\"statusCode\": 200,"
//       "\"headers\": {"
//           "\"Content-Type\": \"application/json\""
//       "},"
//       "\"multiValueHeaders\": {"
//           "\"test-multi-header\": [\"multi-value-1\", \"multi-value-2\"]"
//       "},"
//       "\"body\": {"
//           "\"test\": \"test-value\""
//       "}"
//   "}");

//   ApiGatewayTransformer transformer;
//   NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_{};
//   transformer.transform(response_headers, &headers, body, filter_callbacks_);

//   std::string res = body.toString();
//   json actual = json::parse(res);
//   auto expected = R"(
//   {"test": "test-value"}
// )"_json;

//   EXPECT_EQ(expected, actual);
//   EXPECT_EQ("200", response_headers.getStatusValue());
//   EXPECT_EQ("application/json", response_headers.getContentTypeValue());
//   auto lowercase_multi_header_name = Http::LowerCaseString("test-multi-header");
//   auto header_values = response_headers.get(lowercase_multi_header_name);
//   EXPECT_EQ(header_values.empty(), false);
//   auto str_header_value = header_values[0]->value().getStringView();
//   EXPECT_EQ("multi-value-1,multi-value-2", str_header_value);
// }

TEST(ApiGatewayTransformer, transform_null_multi_value_headers) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {"x-test", "789"},
                                         {":path", "/users/123"}};
  Http::TestResponseHeaderMapImpl response_headers{};
  Buffer::OwnedImpl body("{"
      "\"multiValueHeaders\": {"
          "\"test-multi-header\": [\"multi-value-1\", null]"
      "}"
  "}");

  ApiGatewayTransformer transformer;
  NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_{};
  EXPECT_LOG_CONTAINS(
    "debug",
    "Error transforming response: [json.exception.type_error.302] type must be string, but is null",
    transformer.transform(response_headers, &headers, body, filter_callbacks_)
  );
}

TEST(ApiGatewayTransformer, transform_dangerous_multi_value_header_values) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {"x-test", "789"},
                                         {":path", "/users/123"}};
  Http::TestResponseHeaderMapImpl response_headers{};
  std::map<std::string, std::unique_ptr<Buffer::OwnedImpl>> bodies = {};

  bodies["Null multi-header value"] = std::make_unique<Buffer::OwnedImpl>(R"json({
      "multiValueHeaders": {
          "test-multi-header": ["multi-value-1", null]
      }
    })json");
  bodies["Null multi-header value in middle"] = std::make_unique<Buffer::OwnedImpl>(R"json({
      "multiValueHeaders": {
          "test-multi-header": ["multi-value-1", null, "multi-value-2"]
      }
    })json");
  bodies["int multi-header value"] = std::make_unique<Buffer::OwnedImpl>(R"json({
      "multiValueHeaders": {
          "test-multi-header": ["multi-value-1", 123]
      }
    })json");
  // // this one creates a parse error, not the type error
  // bodies["null initial multi-header value"] = std::make_unique<Buffer::OwnedImpl>(R"json({
  //     "\"multiValueHeaders\": {"
  //         "\"test-multi-header\": [null, \"multi-value-1\"]"
  //     "}"
  //   })json");
  // // also a parse error
  // bodies["null lone multi-header value"] = std::make_unique<Buffer::OwnedImpl>(R"json({
  //     "\"multiValueHeaders\": {"
  //         "\"test-multi-header\": [null]"
  //     "}"
  //   })json");

  for (const auto& [bodyName, bodyPtr] : bodies) {
    ApiGatewayTransformer transformer;
    NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_{};
    std::cout << "Processing body: " + bodyName << std::endl;
    EXPECT_LOG_CONTAINS(
      "debug",
      "Error transforming response: [json.exception.type_error.302] type must be string, but is",
      // "Error transforming response: [json.exception.type_error.302] type must be string, but is null",
      transformer.transform(response_headers, &headers, *bodyPtr, filter_callbacks_)
    );
  }
}

TEST(ApiGatewayTransformer, transform_dangerous_status_code_values) {
    Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {"x-test", "789"},
                                         {":path", "/users/123"}};
  Http::TestResponseHeaderMapImpl response_headers{};
  std::map<std::string, std::unique_ptr<Buffer::OwnedImpl>> bodies = {};

  bodies["string status code"] = std::make_unique<Buffer::OwnedImpl>(R"json({
      "statusCode": "200"
    })json");
  bodies["null status code"] = std::make_unique<Buffer::OwnedImpl>(R"json({
      "statusCode": null
    })json");
  bodies["object status code"] = std::make_unique<Buffer::OwnedImpl>(R"json({
      "statusCode": {"test": "test-value"}
    })json");
  bodies["list status code"] = std::make_unique<Buffer::OwnedImpl>(R"json({
      "statusCode": ["test-valuee"]
    })json");

  for (const auto& [bodyName, bodyPtr] : bodies) {
    ApiGatewayTransformer transformer;
    NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_{};
    std::cout << "Processing body: " + bodyName << std::endl;
    EXPECT_LOG_CONTAINS(
      "debug",
      "Error parsing statusCode: [json.exception.type_error.302] type must be number, but is",
      transformer.transform(response_headers, &headers, *bodyPtr, filter_callbacks_)
    );
  }
}

// TEST(ApiGatewayTransformer, transform_single_and_multi_value_headers) {
//   Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
//                                          {":authority", "www.solo.io"},
//                                          {"x-test", "789"},
//                                          {":path", "/users/123"}};
//   Http::TestResponseHeaderMapImpl response_headers{};
//   Buffer::OwnedImpl body("{"
//       "\"statusCode\": 200,"
//       "\"headers\": {"
//           "\"test-multi-header\": \"multi-value-0\""
//       "},"
//       "\"multiValueHeaders\": {"
//           "\"test-multi-header\": [\"multi-value-1\", \"multi-value-2\"]"
//       "},"
//       "\"body\": {"
//           "\"test\": \"test-value\""
//       "}"
//   "}");

//   ApiGatewayTransformer transformer;
//   NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_{};
//   transformer.transform(response_headers, &headers, body, filter_callbacks_);

//   std::string res = body.toString();
//   json actual = json::parse(res);
//   auto expected = R"(
//   {"test": "test-value"}
// )"_json;

//   EXPECT_EQ(expected, actual);
//   EXPECT_EQ("200", response_headers.getStatusValue());
//   auto lowercase_multi_header_name = Http::LowerCaseString("test-multi-header");
//   auto header_values = response_headers.get(lowercase_multi_header_name);
//   EXPECT_EQ(header_values.empty(), false);
//   auto str_header_value = header_values[0]->value().getStringView();
//   EXPECT_EQ("multi-value-0,multi-value-1,multi-value-2", str_header_value);
// }

// TEST(ApiGatewayTransformer, transform_non_string_headers) {
//   Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
//                                          {":authority", "www.solo.io"},
//                                          {"x-test", "789"},
//                                          {":path", "/users/123"}};
//   Http::TestResponseHeaderMapImpl response_headers{};
//   Buffer::OwnedImpl body("{"
//       "\"statusCode\": 200,"
//       "\"headers\": {"
//           "\"string-header\": \"test_value\","
//           "\"number-header\": 400,"
//           "\"object-header\": {"
//               "\"test\": \"test-value\""
//           "}"
//       "},"
//       "\"body\": {"
//           "\"test\": \"test-value\""
//       "}"
//   "}");

//   ApiGatewayTransformer transformer;
//   NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_{};
//   transformer.transform(response_headers, &headers, body, filter_callbacks_);

//   std::string res = body.toString();
//   json actual = json::parse(res);
//   auto expected = R"(
//   {"test": "test-value"}
// )"_json;

//   EXPECT_EQ(expected, actual);
//   EXPECT_EQ("200", response_headers.getStatusValue());

//   auto lowercase_string_header_name = Http::LowerCaseString("string-header");
//   auto header_values = response_headers.get(lowercase_string_header_name);
//   EXPECT_EQ(header_values.empty(), false);
//   auto str_header_value = header_values[0]->value().getStringView();
//   EXPECT_EQ("test_value", str_header_value);

//   auto lowercase_number_header_name = Http::LowerCaseString("number-header");
//   auto number_header_values = response_headers.get(lowercase_number_header_name);
//   EXPECT_EQ(number_header_values.empty(), false);
//   auto number_header_value = number_header_values[0]->value().getStringView();
//   EXPECT_EQ("400", number_header_value);

//   auto lowercase_object_header_name = Http::LowerCaseString("object-header");
//   auto object_header_values = response_headers.get(lowercase_object_header_name);
//   EXPECT_EQ(object_header_values.empty(), false);
//   auto object_header_value = object_header_values[0]->value().getStringView();
//   EXPECT_EQ("{\"test\":\"test-value\"}", object_header_value);
// }

// TEST(ApiGatewayTransformer, base64decode) {
//   Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
//                                          {":authority", "www.solo.io"},
//                                          {"x-test", "789"},
//                                          {":path", "/users/123"}};
//   Http::TestResponseHeaderMapImpl response_headers{};
//   Buffer::OwnedImpl body("{ \"isBase64Encoded\": true, \"statusCode\": 201,"
//             "\"body\": \"SGVsbG8gZnJvbSBMYW1iZGEgKG9wdGlvbmFsKQ==\"}");

//   ApiGatewayTransformer transformer;
//   NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_{};
//   transformer.transform(response_headers, &headers, body, filter_callbacks_);

//   std::string res = body.toString();

//   EXPECT_EQ("Hello from Lambda (optional)", res);
//   EXPECT_EQ("201", response_headers.getStatusValue());
// }

// TEST(ApiGatewayTransformer, multiple_single_headers) {
//   Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
//                                          {":authority", "www.solo.io"},
//                                          {"x-test", "789"},
//                                          {":path", "/users/123"}};
//   Http::TestResponseHeaderMapImpl response_headers{};
//   Buffer::OwnedImpl body("{"
//       "\"statusCode\": 200,"
//       "\"headers\": {"
//           "\"test-multi-header\": \"multi-value-0\","
//           "\"test-multi-header\": \"multi-value-1\""
//       "},"
//       "\"body\": {"
//           "\"test\": \"test-value\""
//       "}"
//   "}");

//   ApiGatewayTransformer transformer;
//   NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_{};
//   transformer.transform(response_headers, &headers, body, filter_callbacks_);

//   std::string res = body.toString();
//   json actual = json::parse(res);
//   auto expected = R"(
//   {"test": "test-value"}
// )"_json;

//   EXPECT_EQ(expected, actual);
//   EXPECT_EQ("200", response_headers.getStatusValue());
//   auto lowercase_multi_header_name = Http::LowerCaseString("test-multi-header");
//   auto header_values = response_headers.get(lowercase_multi_header_name);
//   EXPECT_EQ(header_values.empty(), false);
//   auto str_header_value = header_values[0]->value().getStringView();
//   EXPECT_EQ("multi-value-1", str_header_value);
// }

// TEST(ApiGatewayTransformer, request_path) {
//   Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
//                                          {":authority", "www.solo.io"},
//                                          {"x-test", "789"},
//                                          {":path", "/users/123"}};
//   Http::TestRequestHeaderMapImpl request_headers{{":method", "GET"},
//                                          {":authority", "www.solo.io"},
//                                          {"x-test", "789"},
//                                          {":path", "/users/123"}};
//   Buffer::OwnedImpl body("{}");
//   ApiGatewayTransformer transformer;
//   NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_{};
//   transformer.transform(request_headers, &headers, body, filter_callbacks_);

//   // Nothing should be transformed in this case -- confirm that the headers and body are unchanged.
//   EXPECT_EQ(headers, request_headers);
//   EXPECT_EQ(body.toString(), "{}");
// }

// TEST(ApiGatewayTransformer, error) {
//   Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
//                                          {":authority", "www.solo.io"},
//                                          {"x-test", "789"},
//                                          {":path", "/users/123"}};
//   Http::TestResponseHeaderMapImpl response_headers{};
//   Buffer::OwnedImpl body("{invalid json}");
//   ApiGatewayTransformer transformer;
//   NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_{};
//   transformer.transform(response_headers, &headers, body, filter_callbacks_);

//   EXPECT_EQ(response_headers.getStatusValue(), "500");
//   EXPECT_EQ(response_headers.get(Http::LowerCaseString("content-type"))[0]->value().getStringView(), "text/plain");
//   EXPECT_EQ(response_headers.get(Http::LowerCaseString("x-amzn-errortype"))[0]->value().getStringView(), "500");
// }

// maybe try throwing in some base64 decoding for fun
// TEST(ApiGatewayTransformer, nonStringEmptyBody) {
//   Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
//                                          {":authority", "www.solo.io"},
//                                          {"x-test", "789"},
//                                          {":path", "/users/123"}};
//   Http::TestResponseHeaderMapImpl response_headers{};
//   Buffer::OwnedImpl body("{"
//       "\"statusCode\": 200,"
//       "\"headers\": {"
//           "\"Content-Type\": \"application/json\""
//       "},"
//       "\"body\": 123"
//   "}");

//   ApiGatewayTransformer transformer;
//   NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_{};
//   transformer.transform(response_headers, &headers, body, filter_callbacks_);

//   std::string res = body.toString();
//   json actual = json::parse(res);
//   auto expected = R"(
//   {}
// )"_json;

//   EXPECT_EQ(expected, actual);
//   EXPECT_EQ("200", response_headers.getStatusValue());
//   EXPECT_EQ("application/json", response_headers.getContentTypeValue());
// }

// test multi header values
// TEST(ApiGatewayTransformer, nullMultiHeaderValues) {
//   Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
//                                          {":authority", "www.solo.io"},
//                                          {"x-test", "789"},
//                                          {":path", "/users/123"}};
//   Http::TestResponseHeaderMapImpl response_headers{};
//   Buffer::OwnedImpl body("{"
//       "\"multiValueHeaders\": {"
//           "\"test-multi-header\": [\"multi-value-1\"]"
//           // "\"test-multi-header\": [\"multi-value-1\", null]"
//       "},"
//   "}");

//   ApiGatewayTransformer transformer;
//   NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_{};
//   transformer.transform(response_headers, &headers, body, filter_callbacks_);

//   auto lowercase_multi_header_name = Http::LowerCaseString("test-multi-header");
//   const auto& header_values = headers.get(lowercase_multi_header_name);
//   EXPECT_EQ(header_values.empty(), false);
//   auto str_header_value = header_values[0]->value().getStringView();
//   EXPECT_EQ("multi-value-1,", str_header_value);
// }

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
