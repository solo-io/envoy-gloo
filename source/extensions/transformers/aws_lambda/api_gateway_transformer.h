#pragma once

#include <map>

#include "source/extensions/filters/http/transformation/transformer.h"
#include "nlohmann/json.hpp"

#include "source/extensions/filters/http/transformation/transformation_filter_config.h"
#include "api/envoy/config/transformer/aws_lambda/v2/api_gateway_transformer.pb.h"
#include "api/envoy/config/transformer/aws_lambda/v2/api_gateway_transformer.pb.validate.h"


namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {

struct ApiGatewayError {
  uint64_t status_code;
  absl::string_view code;
  absl::string_view message;
};

using ApiGatewayTransformerProto =
     envoy::config::transformer::aws_lambda::v2::ApiGatewayTransformation;

class ApiGatewayTransformerFactory
    : public HttpFilters::Transformation::ResponseTransformerExtensionFactory {
public:
  HttpFilters::Transformation::ResponseTransformerConstSharedPtr createResponseTransformer(
      const Protobuf::Message &config,
      Server::Configuration::CommonFactoryContext &context) override;

  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<ApiGatewayTransformerProto>();
  };
  // Factories are no longer found by name, but by ConfigProto type, from
  // `createEmptyConfigProto()`
  std::string name() const override { return "io.solo.api_gateway.api_gateway_transformer"; }
};


class ApiGatewayTransformer : public Transformation::ResponseTransformer, Logger::Loggable<Logger::Id::filter> {
public:
  ApiGatewayTransformer();
  void transform(Http::ResponseHeaderMap &response_headers,
                 Buffer::Instance &body,
                 Http::StreamFilterCallbacks &) const override;
  void transform_response(Http::ResponseHeaderMap &response_headers,
                 Buffer::Instance &body,
                 Http::StreamFilterCallbacks &) const;
  void format_error(Http::ResponseHeaderMap &map,
                 Buffer::Instance &body,
                 ApiGatewayError &error,
                 Http::StreamFilterCallbacks &) const;
  bool passthrough_body() const override { return false; };
private:
  static const Envoy::Http::LowerCaseString AMAZON_ERRORTYPE_HEADER;
  static constexpr uint64_t DEFAULT_STATUS_VALUE = 200;
  static void add_response_header(Http::ResponseHeaderMap &response_headers,
                        absl::string_view header_key,
                        absl::string_view header_value,
                        Http::StreamFilterCallbacks &stream_filter_callbacks,
                        bool append = false);
};

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
