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

  void initFilterWithBodyTemplate(std::string body) {

    auto &transformation = (*config_.mutable_transformations())["abc"];
    transformation.mutable_request_template()->mutable_body()->set_text(body);
    initFilter(); // Re-load config.

    addNameToRoute("abc");
  }

  void addNameToRoute(std::string name) {

    auto &mymeta =
        *(*route_metadata_.mutable_filter_metadata())
             [Config::TransformationMetadataFilters::get().TRANSFORMATION]
                 .mutable_fields();
    mymeta[Config::MetadataTransformationKeys::get().REQUEST_TRANSFORMATION]
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

TEST_F(TransformationFilterTest, TransformsOnHeaders) {
  initFilterWithBodyTemplate("solo");

  EXPECT_CALL(filter_callbacks_, addDecodedData(_, false)).Times(1);
  auto res = filter_->decodeHeaders(headers_, true);
  EXPECT_EQ(FilterHeadersStatus::Continue, res);
}

TEST_F(TransformationFilterTest, ErrorOnBadTemplate) {
  initFilterWithBodyTemplate("{{nonexistentvar}}");

  std::string status;
  EXPECT_CALL(filter_callbacks_, encodeHeaders_(_, _))
      .WillOnce(Invoke([&](HeaderMap &headers, bool) {
        status = headers.Status()->value().c_str();
      }));

  auto res = filter_->decodeHeaders(headers_, true);
  EXPECT_EQ(FilterHeadersStatus::StopIteration, res);
  EXPECT_EQ("400", status);
}

TEST_F(TransformationFilterTest, ErrorOnInvalidJsonBody) {
  initFilterWithBodyTemplate("solo");

  auto resheaders = filter_->decodeHeaders(headers_, false);
  ASSERT_EQ(FilterHeadersStatus::StopIteration, resheaders);

  std::string status;
  EXPECT_CALL(filter_callbacks_, encodeHeaders_(_, _))
      .WillOnce(Invoke([&](HeaderMap &headers, bool) {
        status = headers.Status()->value().c_str();
      }));

  Buffer::OwnedImpl body("this is not json");
  auto res = filter_->decodeData(body, true);
  EXPECT_EQ(FilterDataStatus::StopIterationNoBuffer, res);
  EXPECT_EQ("400", status);
}

TEST_F(TransformationFilterTest, HappyPathWithBody) {
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
