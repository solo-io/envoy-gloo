#include "source/extensions/filters/http/aws_lambda/api_gateway_transformer.h"

#include "source/common/http/headers.h"
#include "source/common/http/utility.h"
#include "source/common/common/base64.h"

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
  auto typed_config =
      MessageUtil::downcastAndValidate<const ApiGatewayTransformerProto &>(
          config, context.messageValidationContext().staticValidationVisitor());
  return std::make_shared<ApiGatewayTransformer>();
}

ApiGatewayTransformer::ApiGatewayTransformer(){}

void ApiGatewayTransformer::format_error(
  Http::ResponseHeaderMap &response_headers,
  Buffer::Instance &body,
  ApiGatewayError &error) const {
    ENVOY_LOG(debug, "Returning error with message: {}", error.message);
    
    // clear existing response headers
    response_headers.clear();

    response_headers.setStatus(error.status_code);
    response_headers.setContentType("text/plain");
    
    auto amazon_errortype_header = Envoy::Http::LowerCaseString("x-amzn-errortype");
    response_headers.setCopy(amazon_errortype_header, error.code);
    body.drain(body.length());
    body.add(error.code + ": " + error.message);
    response_headers.setContentLength(body.length());
  }

void ApiGatewayTransformer::transform(
    Http::RequestOrResponseHeaderMap &header_map,
    Http::RequestHeaderMap *request_headers,
    Buffer::Instance &body,
    Http::StreamFilterCallbacks &) const {
  
  // check if request header map equals response header map
  // catch any resulting exceptions
  if (*request_headers == header_map) {
    // we are on the request path and we don't want to be
    ENVOY_LOG(debug, "Api Gateway transformer cannot be used on the request path");
    throw EnvoyException(
        fmt::format("Api Gateway transformer cannot be used on the request path"));
    return;
  }

  // downcast RequestResponseHeaderMap to ResponseHeaderMap
  Http::ResponseHeaderMap* response_headers = static_cast<Http::ResponseHeaderMap*>(&header_map);

  // all information about the request format is to be contained in the response body
  
  // parse response body as JSON
  auto bodystring = body.toString();
  nlohmann::json json_body;
  try {
    json_body = json::parse(bodystring);
  } catch (std::exception& exception){
    ApiGatewayError error = {400, "400", "failed to parse response body as JSON"};
    return ApiGatewayTransformer::format_error(*response_headers, body, error);
  }

  // set response status code
  if (json_body.contains("statusCode")) {
    uint64_t status_value;
    try {
      status_value = json_body["statusCode"].get<uint64_t>();
    } catch (std::exception& exception){
      ApiGatewayError error = {400, "400", "Non-integer status code"};
      return ApiGatewayTransformer::format_error(*response_headers, body, error);
    }
    response_headers->setStatus(status_value);
  } else {
    response_headers->setStatus(DEFAULT_STATUS_VALUE);
  }

  // set response headers
  if (json_body.contains("headers")) {
    auto headers = json_body["headers"];
    if (!headers.is_object()) {
        ENVOY_LOG(debug, "invalid headers object");
        ApiGatewayError error = {400, "400", "invalid headers object"};
        return ApiGatewayTransformer::format_error(*response_headers, body, error);
    }
    for (json::iterator it = headers.begin(); it != headers.end(); it++) {
        auto header_key = it.key();
        auto header_value = it.value().get<std::string>();
        add_response_header(*response_headers, header_key, header_value, false);
    }
  }

  // set multi-value response headers
  if (json_body.contains("multiValueHeaders")) {
    auto multi_value_headers = json_body["multiValueHeaders"];
    if (!multi_value_headers.is_object()) {
        ENVOY_LOG(debug, "invalid multi headers object");
        ApiGatewayError error = {400, "400", "invalid multi headers object"};
        return ApiGatewayTransformer::format_error(*response_headers, body, error);
    }

    for (json::iterator it = multi_value_headers.begin(); it != multi_value_headers.end(); it++) {
        auto header_key = it.key();
        auto lower_case_header_key = Envoy::Http::LowerCaseString(header_key);
        auto header_values = it.value();

        for (json::iterator inner_it = header_values.begin(); inner_it != header_values.end(); inner_it++) {
          auto header_value = inner_it.value().get<std::string>();
          add_response_header(*response_headers, header_key, header_value, true);
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
      if (json_body["isBase64Encoded"]) {
        body_dump = Base64::decode(body_dump);
      }
    }
    body.add(body_dump);
  } else {
    body.add("{}");
  }
  header_map.setContentLength(body.length());
}

void ApiGatewayTransformer::add_response_header(
  Http::ResponseHeaderMap &response_headers,
  std::string header_key,
  std::string header_value,
  bool append) {
    auto lower_case_header_key = Envoy::Http::LowerCaseString(header_key);
    auto string_header_value = absl::string_view(header_value);
    if (append) {
      response_headers.appendCopy(lower_case_header_key, string_header_value);
    } else {
      response_headers.addCopy(lower_case_header_key, string_header_value);
    }
}

REGISTER_FACTORY(ApiGatewayTransformerFactory,
                 HttpFilters::Transformation::TransformerExtensionFactory);

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
