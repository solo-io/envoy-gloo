
#include "extensions/filters/http/transformation/transformation_filter.h"
#include "extensions/filters/http/solo_well_known_names.h"

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
  enum class ConfigType {
    Listener,
    Route, 
    Both,
  };

  void initFilter() {

    route_config_wrapper_.reset(
        new RouteTransformationFilterConfig(route_config_));
    ON_CALL(*filter_callbacks_.route_,
            perFilterConfig(SoloHttpFilterNames::get().Transformation))
        .WillByDefault(Return(route_config_wrapper_.get()));
    ON_CALL(*encoder_filter_callbacks_.route_,
            perFilterConfig(SoloHttpFilterNames::get().Transformation))
        .WillByDefault(Return(route_config_wrapper_.get()));

    ON_CALL(encoder_filter_callbacks_, route())
        .WillByDefault(Invoke([this]() -> Router::RouteConstSharedPtr {
          if (headers_.Host() == nullptr) {
            throw std::runtime_error("no host");
          }
          return encoder_filter_callbacks_.route_;
        }));
    
    const std::string& stats_prefix = "test_";

    config_ = std::make_shared<TransformationFilterConfig>(listener_config_, stats_prefix, filter_callbacks_.clusterInfo()->statsScope());

    filter_ = std::make_unique<TransformationFilter>(config_);
    filter_->setDecoderFilterCallbacks(filter_callbacks_);
    filter_->setEncoderFilterCallbacks(encoder_filter_callbacks_);
  }

  void initFilterWithBodyTemplate(TransformationFilterTest::ConfigType configType, std::string body) {
    if (configType == TransformationFilterTest::ConfigType::Listener || configType == TransformationFilterTest::ConfigType::Both) {
      auto &transformation = (*listener_config_.mutable_request_transformation());
      transformation.mutable_transformation_template()->mutable_body()->set_text(
          body);
    }  
    if ((configType == TransformationFilterTest::ConfigType::Route || configType == TransformationFilterTest::ConfigType::Both)) {
      auto &transformation = (*route_config_.mutable_request_transformation());
      transformation.mutable_transformation_template()->mutable_body()->set_text(
          body);
    }
    initFilter(); // Re-load config.
  }

  void initFilterWithBodyPassthrough(TransformationFilterTest::ConfigType configType) {
    if (configType == TransformationFilterTest::ConfigType::Listener || configType == TransformationFilterTest::ConfigType::Both) {
      auto &transformation = (*listener_config_.mutable_request_transformation());
      transformation.mutable_transformation_template()->mutable_passthrough();
    }
    if ((configType == TransformationFilterTest::ConfigType::Route || configType == TransformationFilterTest::ConfigType::Both)) {
      auto &transformation = (*route_config_.mutable_request_transformation());
      transformation.mutable_transformation_template()->mutable_passthrough();
    }
    initFilter(); // Re-load config.
  }

  void initFilterWithHeadersBody(TransformationFilterTest::ConfigType configType) {
    if (configType == TransformationFilterTest::ConfigType::Listener || configType == TransformationFilterTest::ConfigType::Both) {
      auto &transformation = (*listener_config_.mutable_request_transformation());
      transformation.mutable_header_body_transform();
    }
    if ((configType == TransformationFilterTest::ConfigType::Route || configType == TransformationFilterTest::ConfigType::Both)) {
      auto &transformation = (*route_config_.mutable_request_transformation());
      transformation.mutable_header_body_transform();
    }
    initFilter(); // Re-load config.
  }

  void transformsOnHeaders(TransformationFilterTest::ConfigType configType, unsigned int val) {
    initFilterWithBodyTemplate(configType, "solo");

    EXPECT_CALL(filter_callbacks_, addDecodedData(_, false)).Times(1);
    EXPECT_CALL(filter_callbacks_, clearRouteCache()).Times(0);
    auto res = filter_->decodeHeaders(headers_, true);
    EXPECT_EQ(Http::FilterHeadersStatus::Continue, res);
    EXPECT_EQ(val, config_->stats().request_header_transformations_.value());
  }

  void transformsOnHeadersAndClearCache(TransformationFilterTest::ConfigType configType, unsigned int val) {
    initFilterWithBodyTemplate(configType, "solo");
    route_config_.set_clear_route_cache(true);
    initFilter(); // Re-load config.

    EXPECT_CALL(filter_callbacks_, addDecodedData(_, false)).Times(1);
    EXPECT_CALL(filter_callbacks_, clearRouteCache()).Times(1);
    auto res = filter_->decodeHeaders(headers_, true);
    EXPECT_EQ(Http::FilterHeadersStatus::Continue, res);
    EXPECT_EQ(val, config_->stats().request_header_transformations_.value());
  }

  void transformsResponseOnHeaders(unsigned int val) {
    route_config_.mutable_response_transformation()
      ->mutable_transformation_template()
      ->mutable_body()
      ->set_text("solo");

    initFilter();

    filter_->decodeHeaders(headers_, true);
    EXPECT_CALL(encoder_filter_callbacks_, addEncodedData(_, false)).Times(1);
    auto res = filter_->encodeHeaders(headers_, true);
    EXPECT_EQ(Http::FilterHeadersStatus::Continue, res);
    EXPECT_EQ(val , config_->stats().response_header_transformations_.value());
  }

  void happyPathWithBody(TransformationFilterTest::ConfigType configType, unsigned int val) {
      initFilterWithBodyTemplate(configType, "{{a}}");

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
    EXPECT_EQ(val, config_->stats().request_body_transformations_.value());
  }
  
  void happyPathWithBodyPassthrough(TransformationFilterTest::ConfigType configType) {
    initFilterWithBodyPassthrough(configType);

    auto resheaders = filter_->decodeHeaders(headers_, false);
    EXPECT_EQ(Http::FilterHeadersStatus::Continue, resheaders);

    Buffer::OwnedImpl downstream_body("{\"a\":\"b\"}");
    auto res = filter_->decodeData(downstream_body, true);
    EXPECT_EQ(Http::FilterDataStatus::Continue, res);
    EXPECT_EQ(0U, config_->stats().request_body_transformations_.value());
  }


  Http::TestHeaderMapImpl headers_{{"content-type", "test"},
                                   {":method", "GET"},
                                   {":authority", "www.solo.io"},
                                   {":path", "/path"}};

  NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_;
  NiceMock<Http::MockStreamEncoderFilterCallbacks> encoder_filter_callbacks_;
  std::unique_ptr<TransformationFilter> filter_;
  RouteTransformationConfigProto route_config_;
  TransformationConfigProto listener_config_;
  FilterConfigSharedPtr config_;
  RouteFilterConfigConstSharedPtr route_config_wrapper_;

};

TEST_F(TransformationFilterTest, EmptyConfig) {
  initFilter();
  auto res = filter_->decodeHeaders(headers_, false);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, res);
}

TEST_F(TransformationFilterTest, TransformsOnHeaders) {
  transformsOnHeaders(TransformationFilterTest::ConfigType::Both, 1U);
  transformsOnHeaders(TransformationFilterTest::ConfigType::Route, 2U);
  transformsOnHeaders(TransformationFilterTest::ConfigType::Listener, 3U);
}

TEST_F(TransformationFilterTest, TransformsOnHeadersAndClearCache) {
  transformsOnHeadersAndClearCache(TransformationFilterTest::ConfigType::Both, 1U);
  transformsOnHeadersAndClearCache(TransformationFilterTest::ConfigType::Route, 2U);
  transformsOnHeadersAndClearCache(TransformationFilterTest::ConfigType::Listener, 3U);
}

TEST_F(TransformationFilterTest, TransformsResponseOnHeaders) {
  transformsResponseOnHeaders(1U);
  transformsResponseOnHeaders(2U);
  transformsResponseOnHeaders(3U);
}

TEST_F(TransformationFilterTest, TransformsResponseOnHeadersNoHost) {
  headers_.remove(":authority");
  route_config_.mutable_response_transformation()
      ->mutable_transformation_template()
      ->mutable_body()
      ->set_text("solo");

  initFilter();

  // no encode headers to simulate local reply error.
  EXPECT_CALL(encoder_filter_callbacks_, addEncodedData(_, false)).Times(0);
  auto res = filter_->encodeHeaders(headers_, true);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, res);
  EXPECT_EQ(0U, config_->stats().response_header_transformations_.value());
}

TEST_F(TransformationFilterTest, ErrorOnBadTemplate) {
  initFilterWithBodyTemplate(TransformationFilterTest::ConfigType::Both, "{{nonexistentvar}}");

  std::string status;
  EXPECT_CALL(filter_callbacks_, encodeHeaders_(_, _))
      .WillOnce(Invoke([&](Http::HeaderMap &headers, bool) {
        status = std::string(headers.Status()->value().getStringView());
      }));

  auto res = filter_->decodeHeaders(headers_, true);
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, res);
  EXPECT_EQ("400", status);
  EXPECT_EQ(1U, config_->stats().request_error_.value());
}

TEST_F(TransformationFilterTest, ErrorOnInvalidJsonBody) {
  initFilterWithBodyTemplate(TransformationFilterTest::ConfigType::Both, "solo");

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
  EXPECT_EQ(1U, config_->stats().request_error_.value());
}

TEST_F(TransformationFilterTest, HappyPathWithBody) {
  happyPathWithBody(TransformationFilterTest::ConfigType::Both, 1U);
  happyPathWithBody(TransformationFilterTest::ConfigType::Route, 2U);
  happyPathWithBody(TransformationFilterTest::ConfigType::Listener, 3U);
}

TEST_F(TransformationFilterTest, HappyPathWithBodyPassthrough) {
  happyPathWithBodyPassthrough(TransformationFilterTest::ConfigType::Both);
  happyPathWithBodyPassthrough(TransformationFilterTest::ConfigType::Route);
  happyPathWithBodyPassthrough(TransformationFilterTest::ConfigType::Listener);
}

TEST_F(TransformationFilterTest, BodyPassthroughDoesntRemoveContentType) {
  auto &transformation = (*route_config_.mutable_request_transformation());
  transformation.mutable_transformation_template()->mutable_passthrough();
  envoy::api::v2::filter::http::InjaTemplate header_value;
  header_value.set_text("added-value");
  (*transformation.mutable_transformation_template()
        ->mutable_headers())["added-header"] = header_value;
  initFilter(); // Re-load config.

  auto resheaders = filter_->decodeHeaders(headers_, false);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, resheaders);

  // as this is a passthrough body, transformation should have been triggered
  // make sure that transformation did not remove content type.
  EXPECT_EQ("test", headers_.get_("content-type"));
  EXPECT_EQ("added-value", headers_.get_("added-header"));
}

TEST_F(TransformationFilterTest, HappyPathWithHeadersBodyTemplate) {
  initFilterWithHeadersBody(TransformationFilterTest::ConfigType::Both);

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
