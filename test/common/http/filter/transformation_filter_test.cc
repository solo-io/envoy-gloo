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

using Http::TransformationFilterConfig;
using Server::Configuration::TransformationFilterConfigFactory;

TEST(TransformationFilterConfigFactory, EmptyConfig) {
  envoy::api::v2::filter::http::Transformations config;

  // shouldnt throw.
  TransformationFilterConfig cfg(config);
  EXPECT_TRUE(cfg.empty());
}

class TransformationFilterTest : public testing::Test {
public:
  void SetUp() override {

    initFilter();
    Router::MockRouteEntry &routerentry =
        filter_callbacks_.route_->route_entry_;
    ON_CALL(routerentry, metadata()).WillByDefault(ReturnRef(route_metadata_));
  }

  void initFilter() {
    Envoy::Http::TransformationFilterConfigSharedPtr configptr(
        new TransformationFilterConfig(config_));
    filter_ = std::make_unique<TransformationFilter>(configptr);
    filter_->setDecoderFilterCallbacks(filter_callbacks_);
  }

  void addNameToRoute(std::string name) {

    auto &mymeta =
        *(*route_metadata_.mutable_filter_metadata())
             [Config::TransformationMetadataFilters::get().TRANSFORMATION]
                 .mutable_fields();
    mymeta[Config::MetadataTransformationKeys::get().TRANSFORMATION]
        .set_string_value(name);
  }

  envoy::api::v2::filter::http::Transformations config_;
  Envoy::Http::TestHeaderMapImpl headers_{
      {":method", "GET"}, {":authority", "www.solo.io"}, {":path", "/path"}};

  NiceMock<Envoy::Http::MockStreamDecoderFilterCallbacks> filter_callbacks_;
  std::unique_ptr<TransformationFilter> filter_;
  envoy::api::v2::core::Metadata route_metadata_;
};

TEST_F(TransformationFilterTest, EmptyConfig) {
  auto res = filter_->decodeHeaders(headers_, true);
  EXPECT_EQ(FilterHeadersStatus::Continue, res);
}

TEST_F(TransformationFilterTest, StopIterationWhenNeedsToTransforms) {
  auto &transformation = (*config_.mutable_transformations())["abc"];
  transformation.mutable_request_template()->mutable_body()->set_text("solo");
  initFilter(); // Re-load config.

  addNameToRoute("abc");

  EXPECT_CALL(filter_callbacks_, addDecodedData(_, false)).Times(1);
  auto res = filter_->decodeHeaders(headers_, true);
  EXPECT_EQ(FilterHeadersStatus::Continue, res);
}

} // namespace Http
} // namespace Envoy
