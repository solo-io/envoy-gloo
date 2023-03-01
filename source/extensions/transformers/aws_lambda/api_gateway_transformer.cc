#include "source/extensions/transformers/aws_lambda/api_gateway_transformer.h"

#include "source/common/http/headers.h"
#include "source/common/http/utility.h"
#include "source/common/common/base64.h"

#include "source/common/http/header_map_impl.h"

#include "nlohmann/json.hpp"
using json = nlohmann::json;


namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {

HttpFilters::Transformation::TransformerConstSharedPtr
ApiGatewayTransformerFactory::createTransformer(
    const Protobuf::Message &config,
    Server::Configuration::CommonFactoryContext &context) {
    MessageUtil::downcastAndValidate<const ApiGatewayTransformerProto &>(
          config, context.messageValidationContext().staticValidationVisitor());
  return std::make_shared<ApiGatewayTransformer>();
}

ApiGatewayTransformer::ApiGatewayTransformer(){}
const Envoy::Http::LowerCaseString ApiGatewayTransformer::AMAZON_ERRORTYPE_HEADER = Envoy::Http::LowerCaseString("x-amzn-errortype");

void ApiGatewayTransformer::format_error(
  Http::ResponseHeaderMap &response_headers,
  Buffer::Instance &body,
  ApiGatewayError &error,
  Http::StreamFilterCallbacks &stream_filter_callbacks) const {
    ENVOY_STREAM_LOG(debug, "Returning error with message: " + std::string(error.message), stream_filter_callbacks);

    // clear existing response headers
    response_headers.clear();

    response_headers.setStatus(error.status_code);
    response_headers.setContentType("text/plain");

    response_headers.setReferenceKey(AMAZON_ERRORTYPE_HEADER, error.code);
    body.drain(body.length());
    auto error_message = absl::StrCat(error.code, ": ", error.message);
    body.add(error_message);
    response_headers.setContentLength(body.length());
  }

void ApiGatewayTransformer::transform(
    Http::RequestOrResponseHeaderMap &header_map,
    Http::RequestHeaderMap *request_headers,
    Buffer::Instance &body,
    Http::StreamFilterCallbacks &stream_filter_callbacks) const {
      ENVOY_STREAM_LOG(debug, "Transforming response", stream_filter_callbacks);
      // check if request header map equals response header map
      if (*request_headers == header_map) {
        // we are on the request path and we don't want to be
        ENVOY_STREAM_LOG(debug, "Api Gateway transformer cannot be used on the request path", stream_filter_callbacks);
        return;
      }
      // we know we are on the repsonse path, so downcast RequestResponseHeaderMap to ResponseHeaderMap
      Http::ResponseHeaderMap* response_headers = static_cast<Http::ResponseHeaderMap*>(&header_map);
      try {
        transform_response(response_headers, body, stream_filter_callbacks);
      } catch (const std::exception &e) {
        ENVOY_STREAM_LOG(debug, "Error transforming response: " + std::string(e.what()), stream_filter_callbacks);
        ApiGatewayError error = ApiGatewayError{500, "500", "Failed to transform response"};
        format_error(*response_headers, body, error, stream_filter_callbacks);
      }
}

void ApiGatewayTransformer::transform_response(
    Http::ResponseHeaderMap *response_headers,
    Buffer::Instance &body,
    Http::StreamFilterCallbacks &stream_filter_callbacks) const {
  // clear existing response headers before any can be set
  // please note: Envoy will crash if the ":status" header is not set
  response_headers->clear();

  // all information about the request format is to be contained in the response body
  // parse response body as JSON
  const auto len = body.length();
  const auto bodystring = absl::string_view(static_cast<char *>(body.linearize(len)), len);
  nlohmann::json json_body;
  try {
    json_body = json::parse(bodystring);
  } catch (std::exception& exception){
    ENVOY_STREAM_LOG(debug, "Error parsing response body as JSON: " + std::string(exception.what()), stream_filter_callbacks);
    ApiGatewayError error = {500, "500", "failed to parse response body as JSON"};
    return ApiGatewayTransformer::format_error(*response_headers, body, error, stream_filter_callbacks);
  }

  // set response status code
  if (json_body.contains("statusCode")) {
    uint64_t status_value;
    try {
      status_value = json_body["statusCode"].get<uint64_t>();
    } catch (std::exception& exception){
      ENVOY_STREAM_LOG(debug, "Error parsing statusCode: " + std::string(exception.what()), stream_filter_callbacks);
      ApiGatewayError error = {500, "500", "Non-integer status code"};
      return ApiGatewayTransformer::format_error(*response_headers, body, error, stream_filter_callbacks);
    }
    response_headers->setStatus(status_value);
  } else {
    response_headers->setStatus(DEFAULT_STATUS_VALUE);
  }

  // set response headers
  if (json_body.contains("headers")) {
    const auto& headers = json_body["headers"];
    if (!headers.is_object()) {
        ENVOY_STREAM_LOG(debug, "invalid headers object", stream_filter_callbacks);
        ApiGatewayError error = {500, "500", "invalid headers object"};
        return ApiGatewayTransformer::format_error(*response_headers, body, error, stream_filter_callbacks);
    }
    for (json::const_iterator it = headers.cbegin(); it != headers.cend(); it++) {
        const auto& header_key = it.key();
        const auto& header_value = it.value();
        std::string header_value_string;
        if (header_value.is_string()) {
          header_value_string = header_value.get<std::string>();
        } else {
          header_value_string = it.value().dump();
        }
        add_response_header(*response_headers, header_key, header_value_string, stream_filter_callbacks, false);
    }
  }

  // set multi-value response headers
  if (json_body.contains("multiValueHeaders")) {
    const auto& multi_value_headers = json_body["multiValueHeaders"];
    if (!multi_value_headers.is_object()) {
        ENVOY_STREAM_LOG(debug, "invalid multi headers object", stream_filter_callbacks);
        ApiGatewayError error = {500, "500", "invalid multi headers object"};
        return ApiGatewayTransformer::format_error(*response_headers, body, error, stream_filter_callbacks);
    }

    for (json::const_iterator it = multi_value_headers.cbegin(); it != multi_value_headers.cend(); it++) {
        const auto& header_key = it.key();
        const auto& header_values = it.value();

        for (json::const_iterator inner_it = header_values.cbegin(); inner_it != header_values.cend(); inner_it++) {
          const auto& header_value = inner_it.value().get<std::string>();
          add_response_header(*response_headers, header_key, header_value, stream_filter_callbacks, true);
        }
    }
  }

  // set response body
  body.drain(body.length());
  if (json_body.contains("body")) {
    std::string body_dump;
    if (json_body["body"].is_string()) {
      body_dump = json_body["body"].get<std::string>();
    } else {
      body_dump = json_body["body"].dump();
    }
    if (json_body.contains("isBase64Encoded")) {
        auto is_base64 = json_body["isBase64Encoded"];
      if (is_base64.is_boolean() && is_base64 == true) {
        body_dump = Base64::decode(body_dump);
      }
    }
    body.add(body_dump);
  } else {
    body.add("{}");
  }
  response_headers->setContentLength(body.length());

  ASSERT(response_headers->getStatusValue() != 0);
}

void ApiGatewayTransformer::add_response_header(
  Http::ResponseHeaderMap &response_headers,
  absl::string_view header_key,
  absl::string_view header_value,
  Http::StreamFilterCallbacks &stream_filter_callbacks,
  bool append) {
    Envoy::Http::LowerCaseString lower_case_header_key(header_key);
    if (!Http::validHeaderString(lower_case_header_key)) {
      ENVOY_STREAM_LOG(debug, "failed to write response header with invalid header key: " + std::string(header_key), stream_filter_callbacks);
      return;
    }

    if (!Http::validHeaderString(header_value)) {
      ENVOY_STREAM_LOG(debug, "failed to write response header with invalid header value: " + std::string(header_value), stream_filter_callbacks);
      return;
    }
    if (append) {
      response_headers.appendCopy(lower_case_header_key, header_value);
    } else {
      response_headers.addCopy(lower_case_header_key, header_value);
    }
}

REGISTER_FACTORY(ApiGatewayTransformerFactory,
                 HttpFilters::Transformation::TransformerExtensionFactory);

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
