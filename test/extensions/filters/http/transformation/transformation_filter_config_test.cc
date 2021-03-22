#include "extensions/filters/http/transformation/transformation_filter_config.h"
#include "test/extensions/filters/http/transformation/fake_transformer.h"
#include "test/test_common/utility.h"
#include "common/config/utility.h"
#include "test/mocks/server/mocks.h"
#include "common/protobuf/protobuf.h"
#include "common/protobuf/utility.h"


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
  auto factoryConfig = std::make_shared<envoy::config::core::v3::TypedExtensionConfig>();
  factoryConfig->set_name("io.solo.transformer.fake");
  transformation.set_allocated_transformer_config(factoryConfig.get());
  auto &factory = Config::Utility::getAndCheckFactory<TransformerExtensionFactory>(transformation.transformer_config());
  EXPECT_EQ(factory.name(), "io.solo.transformer.fake");
}
}
}
}
}