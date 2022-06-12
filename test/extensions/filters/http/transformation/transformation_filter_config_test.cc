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

using FakeTransformerProto = envoy::test::extensions::transformation::FakeTransformer;


TEST(TransformerExtensionFactory, TestTransformerExtensionFactoryRegistration){
  envoy::api::v2::filter::http::Transformation transformation;
  auto factoryConfig = transformation.mutable_transformer_config();
  factoryConfig->set_name("io.solo.transformer.fake");
  auto &factory = Config::Utility::getAndCheckFactoryByName<TransformerExtensionFactory>(transformation.transformer_config());
  EXPECT_EQ(factory.name(), "io.solo.transformer.fake");
}

TEST(Transformation, TestGetTransformer){
  NiceMock<Server::Configuration::MockFactoryContext> factory_context_;

  Transformation t;
  envoy::api::v2::filter::http::Transformation transformation;

  auto factoryConfig = transformation.mutable_transformer_config();
  factoryConfig->set_name("io.solo.transformer.fake");
  auto any = factoryConfig->mutable_typed_config();
  any->set_type_url("type.googleapis.com/envoy.config.transformer.xslt.v2.XsltTransformation");
  auto transformer = t.getTransformer(transformation, factory_context_);
  auto fakeTransformer = dynamic_cast<const Envoy::Extensions::Transformer::Fake::FakeTransformer *>(transformer.get());
  // if transformer is not fake transformer type, will return nullptr
  EXPECT_NE(fakeTransformer, nullptr);
}

}
}
}
}