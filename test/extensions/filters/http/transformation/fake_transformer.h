#include "extensions/filters/http/transformation/transformation_filter_config.h"
#include "test/extensions/filters/http/transformation/fake_transformer.pb.h"

namespace Envoy {
namespace Extensions {
namespace Transformer {
namespace Fake{ 

using namespace HttpFilters::Transformation;
using FakeTransformerProto = envoy::test::extensions::transformation::FakeTransformer;

class FakeTransformer : public HttpFilters::Transformation::Transformer {
public:
  bool passthrough_body() const override {return false;}
  void transform (Http::RequestOrResponseHeaderMap &,
                         // request header map. this has the request header map
                         // even when transforming responses.
                         const Http::RequestHeaderMap *,
                         Buffer::Instance &,
                         Http::StreamFilterCallbacks &) const override {
                           // pass
  }
};

class FakeTransformerFactory : public TransformerExtensionFactory {
public:
  std::string name() const override {return "io.solo.transformer.fake";}

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