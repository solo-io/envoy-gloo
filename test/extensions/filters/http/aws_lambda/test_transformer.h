#include "source/extensions/filters/http/transformation/transformation_filter_config.h"
#include "test/extensions/filters/http/aws_lambda/api_gateway_test_transformer.pb.h"

namespace Envoy {
namespace Extensions {
namespace Transformer {
namespace Fake{ 

using namespace HttpFilters::Transformation;
using FakeTransformerProto = envoy::test::extensions::transformation::ApiGatewayTestTransformer;

class FakeTransformer : public HttpFilters::Transformation::Transformer {
public:
  FakeTransformer() : Transformer(google::protobuf::BoolValue()) {}
  bool passthrough_body() const override {return false;}
  // This transformer just drains the body and replaces it with a hardcoded string.
  void transform (Http::RequestOrResponseHeaderMap &headers,
                         Http::RequestHeaderMap *,
                         Buffer::Instance &body,
                         Http::StreamFilterCallbacks &) const override {
                            std::string headers_string = std::string("headers:\n");
                            headers.iterate(
                                [&headers_string](const Http::HeaderEntry &header) -> Http::HeaderMap::Iterate {
                                    auto key = std::string(header.key().getStringView());
                                    auto value = std::string(header.value().getStringView());
                                    // use semicolon as a separator, because pseudo-headers (e.g. :path) have colons (":") in them
                                    headers_string += std::string("\t") + key + "; " + value + "\n";
                                    return Http::HeaderMap::Iterate::Continue;
                                });

                            std::string bodyString = "test body from fake transformer\n" + headers_string;
                            body.drain(body.length());
                            body.add(bodyString);
  }

};

class FakeTransformerFactory : public TransformerExtensionFactory {
public:
  std::string name() const override {return "io.solo.transformer.api_gateway_test_transformer";}

  TransformerConstSharedPtr createTransformer(const Protobuf::Message &,
   __attribute__((unused)) google::protobuf::BoolValue log_request_response_info,
  Server::Configuration::CommonFactoryContext &) override {
    return std::make_shared<FakeTransformer>();
  }

  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<FakeTransformerProto>();
  }
};

/**
 * Static registration for Fake Transformer. @see RegisterFactory.
 */
REGISTER_FACTORY(FakeTransformerFactory, TransformerExtensionFactory);

} // namespace Fake
} // namespace Transformer
} // namespace Extensions
} // namespace Envoy
