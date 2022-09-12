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
  bool passthrough_body() const override {return false;}
  // This transformer just drains the body and replaces it with a hardcoded string.
  void transform (Http::RequestOrResponseHeaderMap &,
                         Http::RequestHeaderMap *,
                         Buffer::Instance &body,
                         Http::StreamFilterCallbacks &) const override {
                            body.drain(body.length());
                            body.add("test body from fake transformer");
  }

};

class FakeTransformerFactory : public TransformerExtensionFactory {
public:
  std::string name() const override {return "io.solo.transformer.api_gateway_test_transformer";}

  TransformerConstSharedPtr createTransformer(const Protobuf::Message &,
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