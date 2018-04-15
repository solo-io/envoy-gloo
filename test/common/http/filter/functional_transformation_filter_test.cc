#include "common/config/transformation_well_known_names.h"
#include "common/http/filter/transformation_filter.h"

#include "server/config/http/transformation_filter_config_factory.h"

#include "test/mocks/common.h"
#include "test/mocks/server/mocks.h"
#include "test/mocks/upstream/mocks.h"
#include "test/test_common/utility.h"

#include "fmt/format.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::AtLeast;
using testing::Invoke;
using testing::Return;
using testing::ReturnPointee;
using testing::ReturnRef;
using testing::SaveArg;
using testing::WithArg;
using testing::_;

namespace Envoy {
namespace Http {

class FunctionNameMetadataAccessor : public MetadataAccessor {
public:
  virtual absl::optional<const std::string *> getFunctionName() const {
    if (function_name_.empty())
      return {};
    return &function_name_;
  }
  virtual absl::optional<const ProtobufWkt::Struct *> getFunctionSpec() const {
    return {};
  }
  virtual absl::optional<const ProtobufWkt::Struct *>
  getClusterMetadata() const {
    return {};
  }
  virtual absl::optional<const ProtobufWkt::Struct *> getRouteMetadata() const {
    return {};
  }

  virtual ~FunctionNameMetadataAccessor() {}
  std::string function_name_;
};

using Server::Configuration::TransformationFilterConfigFactory;

class FunctionalTransformationFilterTest : public testing::Test {
public:
  void SetUp() override {

    initFilter();
    Router::MockRouteEntry &routerentry =
        filter_callbacks_.route_->route_entry_;
    ON_CALL(routerentry, metadata()).WillByDefault(ReturnRef(route_metadata_));

    // todo - make cluster manager return something with metadata
    // add function name to the route

    cluster_name_ = filter_callbacks_.route_->route_entry_.cluster_name_;
    function_name_ = "funcname";
  }

  void initFilter() {
    TransformationFilterConfigConstSharedPtr configptr(
        new TransformationFilterConfig(config_));
    filter_ = std::make_unique<FunctionalTransformationFilter>(configptr);
    filter_->setDecoderFilterCallbacks(filter_callbacks_);
    fnma_.function_name_ = function_name_;
    filter_->retrieveFunction(fnma_);
  }

  void initFilterWithBodyTemplate(std::string body) {

    auto &transformation = (*config_.mutable_transformations())["abc"];
    transformation.mutable_transformation_template()->mutable_body()->set_text(
        body);
    initFilter(); // Re-load config.

    addNameToRoute("abc");
  }

  void addNameToRoute(std::string name) {

    auto &mymeta =
        *(*route_metadata_.mutable_filter_metadata())
             [Config::TransformationMetadataFilters::get().TRANSFORMATION]
                 .mutable_fields();
    auto *s =
        mymeta[Config::MetadataTransformationKeys::get().REQUEST_TRANSFORMATION]
            .mutable_struct_value();

    auto *cluster_fields = (*s->mutable_fields())[cluster_name_]
                               .mutable_struct_value()
                               ->mutable_fields();

    (*cluster_fields)[function_name_].set_string_value(name);
  }

  envoy::api::v2::filter::http::Transformations config_;
  TestHeaderMapImpl headers_{
      {":method", "GET"}, {":authority", "www.solo.io"}, {":path", "/path"}};

  NiceMock<MockStreamDecoderFilterCallbacks> filter_callbacks_;
  std::unique_ptr<FunctionalTransformationFilter> filter_;
  envoy::api::v2::core::Metadata route_metadata_;
  std::string cluster_name_;
  std::string function_name_;
  FunctionNameMetadataAccessor fnma_;
};

TEST_F(FunctionalTransformationFilterTest, HappyPathWithBody) {
  initFilterWithBodyTemplate("{{a}}");

  auto resheaders = filter_->decodeHeaders(headers_, false);
  ASSERT_EQ(FilterHeadersStatus::StopIteration, resheaders);

  std::string upstream_body;
  EXPECT_CALL(filter_callbacks_, addDecodedData(_, false))
      .WillOnce(Invoke([&](Buffer::Instance &b, bool) {
        upstream_body = TestUtility::bufferToString(b);
      }));

  Buffer::OwnedImpl downstream_body("{\"a\":\"b\"}");
  auto res = filter_->decodeData(downstream_body, true);
  EXPECT_EQ(FilterDataStatus::Continue, res);
  EXPECT_EQ("b", upstream_body);
}

} // namespace Http
} // namespace Envoy
