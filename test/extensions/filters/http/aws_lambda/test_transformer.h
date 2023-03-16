#include "source/extensions/filters/http/transformation/transformation_filter_config.h"
#include "test/extensions/filters/http/aws_lambda/api_gateway_test_transformer.pb.h"

namespace Envoy {
namespace Extensions {
namespace Transformer {
namespace Fake{

using namespace HttpFilters::Transformation;
using FakeTransformerProto = envoy::test::extensions::transformation::ApiGatewayTestTransformer;
using FakeRequestTransformerProto = envoy::test::extensions::transformation::ApiGatewayTestRequestTransformer;
using FakeResponseTransformerProto = envoy::test::extensions::transformation::ApiGatewayTestResponseTransformer;

void fakeTransform(Http::RequestOrResponseHeaderMap &headers,
        Buffer::Instance &body,
        Http::StreamFilterCallbacks &) {
    std::string *headers_string = new std::string("headers:\n");
    headers.iterate(
            [headers_string](const Http::HeaderEntry &header) -> Http::HeaderMap::Iterate {
            auto key = std::string(header.key().getStringView());
            auto value = std::string(header.value().getStringView());
            // use semicolon as a separator, because pseudo-headers (e.g. :path) have colons (":") in them
            *headers_string += "\t" + key + "; " + value + "\n";
            return Http::HeaderMap::Iterate::Continue;
            });

    std::string bodyString = "test body from fake transformer\n" + *headers_string;
    body.drain(body.length());
    body.add(bodyString);
}
class FakeTransformer : public HttpFilters::Transformation::Transformer {
public:
  bool passthrough_body() const override {return false;}
  // This transformer just drains the body and replaces it with a hardcoded string.

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

class FakeRequestTransformer : public RequestTransformer, public FakeTransformer {
public:
  bool passthrough_body() const override {return false;}
  // This transformer just drains the body and replaces it with a hardcoded string.
  void transform (Http::RequestHeaderMap &headers,
                         Buffer::Instance &body,
                         Http::StreamFilterCallbacks &cb) const override {
      fakeTransform(headers, body, cb);
  }

};

class FakeRequestTransformerFactory : public RequestTransformerExtensionFactory {
public:
  std::string name() const override {return "io.solo.requesttransformer.api_gateway_test_transformer";}

  TransformerConstSharedPtr createTransformer(const Protobuf::Message &message,
  Server::Configuration::CommonFactoryContext &context) override {
    return createRequestTransformer(message, context);
  }
  virtual RequestTransformerConstSharedPtr createRequestTransformer(const Protobuf::Message &,
    Server::Configuration::CommonFactoryContext &) override {
    return std::make_shared<FakeRequestTransformer>();
  };


  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<FakeRequestTransformerProto>();
  }
};

class FakeResponseTransformer : public ResponseTransformer, public FakeTransformer {
public:
  bool passthrough_body() const override {return false;}
  // This transformer just drains the body and replaces it with a hardcoded string.
  void transform (Http::ResponseHeaderMap &headers,
                  Http::RequestHeaderMap *,
                         Buffer::Instance &body,
                         Http::StreamFilterCallbacks &cb) const override {
      fakeTransform(headers, body, cb);
  }

};

class FakeResponseTransformerFactory : public ResponseTransformerExtensionFactory {
public:
  std::string name() const override {return "io.solo.responsetransformer.api_gateway_test_transformer";}

  TransformerConstSharedPtr createTransformer(const Protobuf::Message &message,
  Server::Configuration::CommonFactoryContext &context) override {
    return createResponseTransformer(message, context);
  }
  virtual ResponseTransformerConstSharedPtr createResponseTransformer(const Protobuf::Message &,
    Server::Configuration::CommonFactoryContext &) override {
    return std::make_shared<FakeResponseTransformer>();
  };


  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<FakeResponseTransformerProto>();
  }
};

/**
 * Static registration for Fake Transformer. @see RegisterFactory.
 */
REGISTER_FACTORY(FakeTransformerFactory, TransformerExtensionFactory);
REGISTER_FACTORY(FakeRequestTransformerFactory, RequestTransformerExtensionFactory);
REGISTER_FACTORY(FakeResponseTransformerFactory, ResponseTransformerExtensionFactory);

} // namespace Fake
} // namespace Transformer
} // namespace Extensions
} // namespace Envoy
