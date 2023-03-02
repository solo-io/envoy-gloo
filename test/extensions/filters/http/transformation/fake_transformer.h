#include "source/extensions/filters/http/transformation/transformation_filter_config.h"
#include "test/extensions/filters/http/transformation/fake_transformer.pb.h"

namespace Envoy {
namespace Extensions {
namespace Transformer {
namespace Fake{

using namespace HttpFilters::Transformation;
using FakeTransformerProto = envoy::test::extensions::transformation::FakeTransformer;
using FakeRequestTransformerProto = envoy::test::extensions::transformation::FakeRequestTransformer;
using FakeResponseTransformerProto = envoy::test::extensions::transformation::FakeResponseTransformer;
using FakeOnStreamCompleteTransformerProto = envoy::test::extensions::transformation::FakeOnStreamCompleteTransformer;

class FakeTransformer : public HttpFilters::Transformation::Transformer {
public:
  bool passthrough_body() const override {return false;}
  void transform (Http::RequestOrResponseHeaderMap &,
                         // request header map. this has the request header map
                         // even when transforming responses.
                         Http::RequestHeaderMap *,
                         Buffer::Instance &,
                         Http::StreamFilterCallbacks &) const override {
  }

};

class FakeRequestTransformer : public FakeTransformer, public RequestTransformer {
public:
  bool passthrough_body() const override {return false;}
  void transform (Http::RequestOrResponseHeaderMap &,
                         // request header map. this has the request header map
                         // even when transforming responses.
                         Http::RequestHeaderMap *,
                         Buffer::Instance &,
                         Http::StreamFilterCallbacks &) const override {
  }

};

class FakeResponseTransformer : public FakeTransformer, public ResponseTransformer {
public:
  bool passthrough_body() const override {return false;}
  void transform (Http::RequestOrResponseHeaderMap &,
                         // request header map. this has the request header map
                         // even when transforming responses.
                         Http::RequestHeaderMap *,
                         Buffer::Instance &,
                         Http::StreamFilterCallbacks &) const override {
  }

};

class FakeOnStreamCompleteTransformer : public FakeTransformer, public OnStreamCompleteTransformer {
public:
  bool passthrough_body() const override {return false;}
  void transform (Http::RequestOrResponseHeaderMap &,
                         // request header map. this has the request header map
                         // even when transforming responses.
                         Http::RequestHeaderMap *,
                         Buffer::Instance &,
                         Http::StreamFilterCallbacks &) const override {
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

class FakeRequestTransformerExtensionFactory :  public RequestTransformerExtensionFactory {
public:
  std::string name() const override {return "io.solo.requesttransformer.fake";}
  ~FakeRequestTransformerExtensionFactory() override = default;

  virtual TransformerConstSharedPtr createTransformer(const Protobuf::Message &config,
    Server::Configuration::CommonFactoryContext &context) override {
      return createRequestTransformer(config, context);
  };
  virtual RequestTransformerConstSharedPtr createRequestTransformer(const Protobuf::Message &,
    Server::Configuration::CommonFactoryContext &) override {
    return std::make_shared<FakeRequestTransformer>();
  };

  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<FakeRequestTransformerProto>();
  }
};

class FakeResponseTransformerExtensionFactory :  public ResponseTransformerExtensionFactory {
public:
  std::string name() const override {return "io.solo.responsetransformer.fake";}
  ~FakeResponseTransformerExtensionFactory() override = default;

  virtual TransformerConstSharedPtr createTransformer(const Protobuf::Message &config,
    Server::Configuration::CommonFactoryContext &context) override {
      return createResponseTransformer(config, context);
  };
  virtual ResponseTransformerConstSharedPtr createResponseTransformer(const Protobuf::Message &,
    Server::Configuration::CommonFactoryContext &) override {
    return std::make_shared<FakeResponseTransformer>();
  };

  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<FakeResponseTransformerProto>();
  }
};

class FakeOnStreamCompleteTransformerExtensionFactory :  public OnStreamCompleteTransformerExtensionFactory {
public:
  std::string name() const override {return "io.solo.onstreamcompletetransformer.fake";}
  ~FakeOnStreamCompleteTransformerExtensionFactory() override = default;

  virtual TransformerConstSharedPtr createTransformer(const Protobuf::Message &config,
    Server::Configuration::CommonFactoryContext &context) override {
      return createOnStreamCompleteTransformer(config, context);
  };
  virtual OnStreamCompleteTransformerConstSharedPtr createOnStreamCompleteTransformer(const Protobuf::Message &,
    Server::Configuration::CommonFactoryContext &) override {
    return std::make_shared<FakeOnStreamCompleteTransformer>();
  };

  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<FakeOnStreamCompleteTransformerProto>();
  }
};


/**
 * Static registration for Fake Transformer. @see RegisterFactory.
 */
REGISTER_FACTORY(FakeTransformerFactory, TransformerExtensionFactory);
REGISTER_FACTORY(FakeRequestTransformerExtensionFactory, RequestTransformerExtensionFactory);
REGISTER_FACTORY(FakeResponseTransformerExtensionFactory, ResponseTransformerExtensionFactory);
REGISTER_FACTORY(FakeOnStreamCompleteTransformerExtensionFactory, OnStreamCompleteTransformerExtensionFactory);
} // namespace Fake
} // namespace Transformer
} // namespace Extensions
} // namespace Envoy
