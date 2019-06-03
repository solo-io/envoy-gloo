#include "extensions/filters/http/solo_well_known_names.h"
#include "extensions/filters/http/transformation/transformation_filter.h"

#include "test/mocks/common.h"
#include "test/mocks/server/mocks.h"
#include "test/mocks/upstream/mocks.h"

#include "fmt/format.h"
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

class TransformationFilterTest : public testing::Test {
public:
  void initFilter() {

    route_config_wrapper_.reset(
        new RouteTransformationFilterConfig(route_config_));
    ON_CALL(*filter_callbacks_.route_,
            perFilterConfig(SoloHttpFilterNames::get().Transformation))
        .WillByDefault(Return(route_config_wrapper_.get()));
    ON_CALL(*encoder_filter_callbacks_.route_,
            perFilterConfig(SoloHttpFilterNames::get().Transformation))
        .WillByDefault(Return(route_config_wrapper_.get()));

    filter_ = std::make_unique<TransformationFilter>();
    filter_->setDecoderFilterCallbacks(filter_callbacks_);
    filter_->setEncoderFilterCallbacks(encoder_filter_callbacks_);
  }

  void initFilterWithBodyTemplate(std::string body) {

    auto &transformation = (*route_config_.mutable_request_transformation());
    transformation.mutable_transformation_template()->mutable_body()->set_text(
        body);
    initFilter(); // Re-load config.
  }

  void initFilterWithBodyPassthrough() {

    auto &transformation = (*route_config_.mutable_request_transformation());
    transformation.mutable_transformation_template()->mutable_passthrough();
    initFilter(); // Re-load config.
  }

  void initFilterWithHeadersBody() {

    auto &transformation = (*route_config_.mutable_request_transformation());
    transformation.mutable_header_body_transform();
    initFilter(); // Re-load config.
  }

  Http::TestHeaderMapImpl headers_{{"content-type", "test"},
      {":method", "GET"}, {":authority", "www.solo.io"}, {":path", "/path"}};

  NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_;
  NiceMock<Http::MockStreamEncoderFilterCallbacks> encoder_filter_callbacks_;
  std::unique_ptr<TransformationFilter> filter_;
  envoy::api::v2::filter::http::RouteTransformations route_config_;
  RouteTransformationFilterConfigConstSharedPtr route_config_wrapper_;
};

TEST_F(TransformationFilterTest, EmptyConfig) {
  initFilter();
  auto res = filter_->decodeHeaders(headers_, false);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, res);
}

TEST_F(TransformationFilterTest, TransformsOnHeaders) {
  initFilterWithBodyTemplate("solo");

  EXPECT_CALL(filter_callbacks_, addDecodedData(_, false)).Times(1);
  EXPECT_CALL(filter_callbacks_, clearRouteCache()).Times(0);
  auto res = filter_->decodeHeaders(headers_, true);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, res);
}

TEST_F(TransformationFilterTest, TransformsOnHeadersAndClearCache) {
  initFilterWithBodyTemplate("solo");
  route_config_.set_clear_route_cache(true);
  initFilter(); // Re-load config.

  EXPECT_CALL(filter_callbacks_, addDecodedData(_, false)).Times(1);
  EXPECT_CALL(filter_callbacks_, clearRouteCache()).Times(1);
  auto res = filter_->decodeHeaders(headers_, true);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, res);
}

TEST_F(TransformationFilterTest, TransformsResponseOnHeaders) {

  route_config_.mutable_response_transformation()
      ->mutable_transformation_template()
      ->mutable_body()
      ->set_text("solo");

  initFilter();

  filter_->decodeHeaders(headers_, true);
  EXPECT_CALL(encoder_filter_callbacks_, addEncodedData(_, false)).Times(1);
  auto res = filter_->encodeHeaders(headers_, true);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, res);
}

TEST_F(TransformationFilterTest, ErrorOnBadTemplate) {
  initFilterWithBodyTemplate("{{nonexistentvar}}");

  std::string status;
  EXPECT_CALL(filter_callbacks_, encodeHeaders_(_, _))
      .WillOnce(Invoke([&](Http::HeaderMap &headers, bool) {
        status = std::string(headers.Status()->value().getStringView());
      }));

  auto res = filter_->decodeHeaders(headers_, true);
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, res);
  EXPECT_EQ("400", status);
}

TEST_F(TransformationFilterTest, ErrorOnInvalidJsonBody) {
  initFilterWithBodyTemplate("solo");

  auto resheaders = filter_->decodeHeaders(headers_, false);
  ASSERT_EQ(Http::FilterHeadersStatus::StopIteration, resheaders);

  std::string status;
  EXPECT_CALL(filter_callbacks_, encodeHeaders_(_, _))
      .WillOnce(Invoke([&](Http::HeaderMap &headers, bool) {
        status = std::string(headers.Status()->value().getStringView());
      }));

  Buffer::OwnedImpl body("this is not json");
  auto res = filter_->decodeData(body, true);
  EXPECT_EQ(Http::FilterDataStatus::StopIterationNoBuffer, res);
  EXPECT_EQ("400", status);
}

TEST_F(TransformationFilterTest, HappyPathWithBody) {
  initFilterWithBodyTemplate("{{a}}");

  auto resheaders = filter_->decodeHeaders(headers_, false);
  ASSERT_EQ(Http::FilterHeadersStatus::StopIteration, resheaders);

  std::string upstream_body;
  EXPECT_CALL(filter_callbacks_, addDecodedData(_, false))
      .WillOnce(Invoke(
          [&](Buffer::Instance &b, bool) { upstream_body = b.toString(); }));

  Buffer::OwnedImpl downstream_body("{\"a\":\"b\"}");
  auto res = filter_->decodeData(downstream_body, true);
  EXPECT_EQ(Http::FilterDataStatus::Continue, res);
  EXPECT_EQ("b", upstream_body);
}

TEST_F(TransformationFilterTest, HappyPathWithBodyPassthrough) {
  initFilterWithBodyPassthrough();

  auto resheaders = filter_->decodeHeaders(headers_, false);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, resheaders);

  Buffer::OwnedImpl downstream_body("{\"a\":\"b\"}");
  auto res = filter_->decodeData(downstream_body, true);
  EXPECT_EQ(Http::FilterDataStatus::Continue, res);
}


TEST_F(TransformationFilterTest, BodyPassthroughDoesntRemoveContentType) {
  auto &transformation = (*route_config_.mutable_request_transformation());
  transformation.mutable_transformation_template()->mutable_passthrough();
  envoy::api::v2::filter::http::InjaTemplate header_value;
  header_value.set_text("added-value");
  (*transformation.mutable_transformation_template()->mutable_headers())["added-header"] = header_value;
  initFilter(); // Re-load config.

  auto resheaders = filter_->decodeHeaders(headers_, false);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, resheaders);

  // as this is a passthrough body, transformation should have been triggered
  // make sure that transformation did not remove content type.
  EXPECT_EQ("test", headers_.get_("content-type"));
  EXPECT_EQ("added-value", headers_.get_("added-header"));
}

TEST_F(TransformationFilterTest, HappyPathWithHeadersBodyTemplate) {
  initFilterWithHeadersBody();

  auto resheaders = filter_->decodeHeaders(headers_, false);
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, resheaders);

  Buffer::OwnedImpl downstream_body("testbody");
  auto res = filter_->decodeData(downstream_body, true);
  EXPECT_EQ(Http::FilterDataStatus::Continue, res);
}

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
