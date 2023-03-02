#include "source/extensions/filters/http/transformation/transformation_filter_config.h"
#include "test/extensions/filters/http/transformation/fake_transformer.h"
#include "test/test_common/utility.h"
#include "source/common/config/utility.h"
#include "test/mocks/server/mocks.h"
#include "source/common/protobuf/protobuf.h"
#include "source/common/protobuf/utility.h"


#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::_;
using testing::AtLeast;
using testing::Invoke;
using testing::Return;
using testing::ReturnPointee;
using testing::ReturnRef;
using testing::SaveArg;
using testing::WithArg;


namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

TEST(TransformerExtensionFactory, TestTransformerExtensionFactoryRegistration){
  envoy::api::v2::filter::http::Transformation transformation;
  auto factoryConfig = transformation.mutable_transformer_config();
  factoryConfig->set_name("io.solo.transformer.fake");
  auto any = factoryConfig->mutable_typed_config();
  any->set_type_url("type.googleapis.com/envoy.test.extensions.transformation.FakeTransformer");
  auto &factory = Config::Utility::getAndCheckFactory<TransformerExtensionFactory>(transformation.transformer_config());
  EXPECT_EQ(factory.name(), "io.solo.transformer.fake");
}

TEST(TransformerExtensionFactory, TestRequestTransformerExtensionFactoryRegistration){
  envoy::api::v2::filter::http::Transformation transformation;
  auto factoryConfig = transformation.mutable_transformer_config();
  factoryConfig->set_name("io.solo.requesttransformer.fake");
  auto any = factoryConfig->mutable_typed_config();
  any->set_type_url("type.googleapis.com/envoy.test.extensions.transformation.FakeRequestTransformer");
  auto &factory = Config::Utility::getAndCheckFactory<RequestTransformerExtensionFactory>(transformation.transformer_config());
  EXPECT_EQ(factory.name(), "io.solo.requesttransformer.fake");
}

TEST(TransformerExtensionFactory, TestResponseTransformerExtensionFactoryRegistration){
  envoy::api::v2::filter::http::Transformation transformation;
  auto factoryConfig = transformation.mutable_transformer_config();
  factoryConfig->set_name("io.solo.responsetransformer.fake");
  auto any = factoryConfig->mutable_typed_config();
  any->set_type_url("type.googleapis.com/envoy.test.extensions.transformation.FakeResponseTransformer");
  auto &factory = Config::Utility::getAndCheckFactory<ResponseTransformerExtensionFactory>(transformation.transformer_config());
  EXPECT_EQ(factory.name(), "io.solo.responsetransformer.fake");
}

TEST(TransformerExtensionFactory, TestOnStreamCompleteTransformerExtensionFactoryRegistration){
  envoy::api::v2::filter::http::Transformation transformation;
  auto factoryConfig = transformation.mutable_transformer_config();
  factoryConfig->set_name("io.solo.onstreamcompletetransformer.fake");
  auto any = factoryConfig->mutable_typed_config();
  any->set_type_url("type.googleapis.com/envoy.test.extensions.transformation.FakeOnStreamCompleteTransformer");
  auto &factory = Config::Utility::getAndCheckFactory<OnStreamCompleteTransformerExtensionFactory>(transformation.transformer_config());
  EXPECT_EQ(factory.name(), "io.solo.onstreamcompletetransformer.fake");
}

TEST(Transformation, TestGetRequestTransformer){
  NiceMock<Server::Configuration::MockFactoryContext> factory_context_;

  RequestTransformation t;
  envoy::api::v2::filter::http::Transformation transformation;

  auto factoryConfig = transformation.mutable_transformer_config();
  factoryConfig->set_name("io.solo.requesttransformer.fake");
  auto any = factoryConfig->mutable_typed_config();
  any->set_type_url("type.googleapis.com/envoy.test.extensions.transformation.FakeRequestTransformer");
  auto transformer = t.getTransformer(transformation, factory_context_);
  auto fakeTransformer = dynamic_cast<const Envoy::Extensions::Transformer::Fake::FakeTransformer *>(transformer.get());
  // if transformer is not fake transformer type, will return nullptr
  EXPECT_NE(fakeTransformer, nullptr);
}

TEST(Transformation, TestGetResponseTransformer){
  NiceMock<Server::Configuration::MockFactoryContext> factory_context_;

  ResponseTransformation t;
  envoy::api::v2::filter::http::Transformation transformation;

  auto factoryConfig = transformation.mutable_transformer_config();
  factoryConfig->set_name("io.solo.responsetransformer.fake");
  auto any = factoryConfig->mutable_typed_config();
  any->set_type_url("type.googleapis.com/envoy.test.extensions.transformation.FakeResponseTransformer");
  auto transformer = t.getTransformer(transformation, factory_context_);
  auto fakeTransformer = dynamic_cast<const Envoy::Extensions::Transformer::Fake::FakeTransformer *>(transformer.get());
  // if transformer is not fake transformer type, will return nullptr
  EXPECT_NE(fakeTransformer, nullptr);
}

TEST(Transformation, TestGetOnStreamCompleteTransformer){
  NiceMock<Server::Configuration::MockFactoryContext> factory_context_;

  OnStreamCompleteTransformation t;
  envoy::api::v2::filter::http::Transformation transformation;

  auto factoryConfig = transformation.mutable_transformer_config();
  factoryConfig->set_name("io.solo.onstreamcompletetransformer.fake");
  auto any = factoryConfig->mutable_typed_config();
  any->set_type_url("type.googleapis.com/envoy.test.extensions.transformation.FakeOnStreamCompleteTransformer");
  auto transformer = t.getTransformer(transformation, factory_context_);
  auto fakeTransformer = dynamic_cast<const Envoy::Extensions::Transformer::Fake::FakeTransformer *>(transformer.get());
  // if transformer is not fake transformer type, will return nullptr
  EXPECT_NE(fakeTransformer, nullptr);
}

TEST(Transformation, TestMismatchedTransformer){
  NiceMock<Server::Configuration::MockFactoryContext> factory_context_;

  RequestTransformation t;
  envoy::api::v2::filter::http::Transformation transformation;

  auto factoryConfig = transformation.mutable_transformer_config();
  factoryConfig->set_name("io.solo.responsetransformer.fake");
  auto any = factoryConfig->mutable_typed_config();
  any->set_type_url("type.googleapis.com/envoy.test.extensions.transformation.FakeResponseTransformer");

  EXPECT_THROW(t.getTransformer(transformation, factory_context_), EnvoyException);
}

}
}
}
}
