// NTS: move this to enterprise eventually

#pragma once

#include <map>

#include "source/extensions/filters/http/transformation/transformer.h"
#include "nlohmann/json.hpp"

#include "api/envoy/config/filter/http/aws_lambda/v2/aws_lambda.pb.validate.h"
#include "source/extensions/filters/http/transformation/transformation_filter_config.h"


namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {

struct ApiGatewayError {
  uint64_t status_code;
  std::string code;
  std::string message;
};

using ApiGatewayTransformerProto =
     envoy::config::filter::http::aws_lambda::v2::ApiGatewayTransformation;
class ApiGatewayTransformerFactory
    : public HttpFilters::Transformation::TransformerExtensionFactory {
public:
  HttpFilters::Transformation::TransformerConstSharedPtr createTransformer(
      const Protobuf::Message &config,
      Server::Configuration::CommonFactoryContext &context) override;

  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<ApiGatewayTransformerProto>();
  };
  // Factories are no longer found by name, but by ConfigProto type, from
  // `createEmptyConfigProto()`
  std::string name() const override { return "io.solo.api_gateway.api_gateway_transformer"; }
};


class ApiGatewayTransformer : public Transformation::Transformer, Logger::Loggable<Logger::Id::filter> {
public:
// NTS: inherit from logger
  ApiGatewayTransformer();
  void transform(Http::RequestOrResponseHeaderMap &map,
                 Http::RequestHeaderMap *request_headers,
                 Buffer::Instance &body,
                 Http::StreamFilterCallbacks &) const override;
  void format_error(Http::ResponseHeaderMap &map,
                 Buffer::Instance &body,
                 ApiGatewayError &error) const;
  bool passthrough_body() const override { return false; };

  uint64_t DEFAULT_STATUS_VALUE = 200;

};


void add_response_header(
  Http::ResponseHeaderMap &response_headers,
  std::string header_key,
  std::string header_value,
  bool append
);


} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
