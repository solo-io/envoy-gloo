#include "source/extensions/filters/http/solo_well_known_names.h"
#include "source/extensions/filters/http/transformation/transformation_filter.h"

#include "test/mocks/common.h"
#include "test/mocks/server/mocks.h"
#include "test/mocks/upstream/mocks.h"
#include "source/common/common/logger.h"
#include "test/mocks/server/factory_context.h"

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
using testing::Throw;
using testing::WithArg;

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

const std::string invalid_template = "{{not a valid template";
std::string get_invalid_template_error(std::string request_or_response) {
  auto error_pos = std::to_string(invalid_template.length()+1);
  return "Failed to parse " + request_or_response + " template: "
    "Failed to parse body template "
    "[inja.exception.parser_error] (at 1:" + error_pos + ") malformed expression";
}

TEST(TransformationFilterConfig, TransformationUpstreamFactoryTest) {

  auto* factory =
      Registry::FactoryRegistry<Server::Configuration::UpstreamHttpFilterConfigFactory>::getFactory(
          "io.solo.transformation");
  ASSERT_NE(factory, nullptr);
}

TEST(TransformationFilterConfig, EnvoyExceptionOnBadRouteConfig) {
  NiceMock<Server::Configuration::MockServerFactoryContext> server_factory_context_;
  NiceMock<Stats::MockIsolatedStatsStore> scope;
  envoy::api::v2::filter::http::TransformationRule transformation_rule;
  auto &route_matcher = (*transformation_rule.mutable_match());
  route_matcher.set_prefix("/");
  {
    auto &transformation =
        (*transformation_rule.mutable_route_transformations()
              ->mutable_request_transformation());
    transformation.mutable_transformation_template()->mutable_body()->set_text(
        invalid_template);

    TransformationConfigProto listener_config;
    *listener_config.mutable_transformations()->Add() = transformation_rule;

    EXPECT_THROW_WITH_MESSAGE(
        std::make_unique<TransformationFilterConfig>(listener_config, "foo",
                                                     server_factory_context_),
        EnvoyException,
        get_invalid_template_error("request"));
  }
  transformation_rule.mutable_route_transformations()->Clear();
  {
    auto &transformation =
        (*transformation_rule.mutable_route_transformations()
              ->mutable_response_transformation());
    transformation.mutable_transformation_template()->mutable_body()->set_text(
        invalid_template);

    TransformationConfigProto listener_config;
    *listener_config.mutable_transformations()->Add() = transformation_rule;

    EXPECT_THROW_WITH_MESSAGE(
        std::make_unique<TransformationFilterConfig>(listener_config, "foo",
                                                     server_factory_context_),
        EnvoyException,
        get_invalid_template_error("response"));
  }
}

TEST(RouteTransformationFilterConfig, EnvoyExceptionOnBadRouteConfig) {
      NiceMock<Server::Configuration::MockServerFactoryContext> server_factory_context_;

  {
    RouteTransformationConfigProto route_config;
    auto &transformation = (*route_config.mutable_request_transformation());
    transformation.mutable_transformation_template()->mutable_body()->set_text(
        invalid_template);

    EXPECT_THROW_WITH_MESSAGE(
        std::make_unique<RouteTransformationFilterConfig>(route_config, server_factory_context_),
        EnvoyException,
        get_invalid_template_error("request"));
  }
  {
    RouteTransformationConfigProto route_config;
    auto &transformation = (*route_config.mutable_response_transformation());
    transformation.mutable_transformation_template()->mutable_body()->set_text(
        invalid_template);

    EXPECT_THROW_WITH_MESSAGE(
        std::make_unique<RouteTransformationFilterConfig>(route_config, server_factory_context_),
        EnvoyException,
        get_invalid_template_error("response"));
  }
}

class TransformationFilterTest : public testing::Test {
public:
  NiceMock<Server::Configuration::MockServerFactoryContext> server_factory_context_;

  enum class ConfigType {
    Listener,
    Route,
    Both,
  };

  void initFilter() {

    *listener_config_.mutable_transformations()->Add() = transformation_rule_;

    route_config_wrapper_.reset(
        new RouteTransformationFilterConfig(route_config_, server_factory_context_));

    if (!null_route_config_) {
      ON_CALL(filter_callbacks_,
              mostSpecificPerFilterConfig())
          .WillByDefault(Return(route_config_wrapper_.get()));
      ON_CALL(encoder_filter_callbacks_,
              mostSpecificPerFilterConfig())
          .WillByDefault(Return(route_config_wrapper_.get()));
    } else {
      ON_CALL(filter_callbacks_,
              mostSpecificPerFilterConfig())
          .WillByDefault(Return(nullptr));
      ON_CALL(encoder_filter_callbacks_,
              mostSpecificPerFilterConfig())
          .WillByDefault(Return(nullptr));
    }

    ON_CALL(encoder_filter_callbacks_, route())
        .WillByDefault(Invoke([this]() -> Router::RouteConstSharedPtr {
          if (headers_.Host() == nullptr) {
            throw std::runtime_error("no host");
          }
          return encoder_filter_callbacks_.route_;
        }));

    const std::string &stats_prefix = "test_";
    config_ = std::make_shared<TransformationFilterConfig>(
        listener_config_, stats_prefix,
        server_factory_context_);

    filter_ = std::make_unique<TransformationFilter>(config_);
    filter_->setDecoderFilterCallbacks(filter_callbacks_);
    filter_->setEncoderFilterCallbacks(encoder_filter_callbacks_);
  }

  void
  initFilterWithBodyTemplate(TransformationFilterTest::ConfigType configType,
                             std::string body) {
    if (configType == TransformationFilterTest::ConfigType::Listener ||
        configType == TransformationFilterTest::ConfigType::Both) {
      auto &transformation =
          (*transformation_rule_.mutable_route_transformations()
                ->mutable_request_transformation());
      transformation.mutable_transformation_template()
          ->mutable_body()
          ->set_text(body);
    }
    if ((configType == TransformationFilterTest::ConfigType::Route ||
         configType == TransformationFilterTest::ConfigType::Both)) {
      auto &transformation = (*route_config_.mutable_request_transformation());
      transformation.mutable_transformation_template()
          ->mutable_body()
          ->set_text(body);
    }
    initFilter(); // Re-load config.
  }

  void initFilterWithBodyPassthrough(
      TransformationFilterTest::ConfigType configType) {
    if (configType == TransformationFilterTest::ConfigType::Listener ||
        configType == TransformationFilterTest::ConfigType::Both) {
      auto &transformation =
          (*transformation_rule_.mutable_route_transformations()
                ->mutable_request_transformation());
      transformation.mutable_transformation_template()->mutable_passthrough();
    }
    if ((configType == TransformationFilterTest::ConfigType::Route ||
         configType == TransformationFilterTest::ConfigType::Both)) {
      auto &transformation = (*route_config_.mutable_request_transformation());
      transformation.mutable_transformation_template()->mutable_passthrough();
    }
    initFilter(); // Re-load config.
  }

  void
  initFilterWithHeadersBody(TransformationFilterTest::ConfigType configType) {
    if (configType == TransformationFilterTest::ConfigType::Listener ||
        configType == TransformationFilterTest::ConfigType::Both) {
      auto &transformation =
          (*transformation_rule_.mutable_route_transformations()
                ->mutable_request_transformation());
      transformation.mutable_header_body_transform();
    }
    if ((configType == TransformationFilterTest::ConfigType::Route ||
         configType == TransformationFilterTest::ConfigType::Both)) {
      auto &transformation = (*route_config_.mutable_request_transformation());
      transformation.mutable_header_body_transform();
    }
    initFilter(); // Re-load config.
  }

  void initOnStreamCompleteTransformHeader() {
    auto &route_matcher = (*transformation_rule_.mutable_match());
    route_matcher.set_prefix("/");
    null_route_config_ = true;
    auto &transformation =
        (*transformation_rule_.mutable_route_transformations()
            ->mutable_on_stream_completion_transformation());
    transformation.mutable_transformation_template()->mutable_passthrough();
    envoy::api::v2::filter::http::InjaTemplate header_value;
    header_value.set_text("added-value");
    (*transformation.mutable_transformation_template()
          ->mutable_headers())["added-header"] = header_value;
    initFilter(); // Re-load config.
  }

  void addMatchersToListenerFilter(const std::string match_string) {
    auto &route_matcher = (*transformation_rule_.mutable_match());
    TestUtility::loadFromYaml(match_string, route_matcher);
  }

  void transformsOnHeaders(TransformationFilterTest::ConfigType configType,
                           unsigned int val) {
    initFilterWithBodyTemplate(configType, "solo");
    int decoded_data_calls = val == 0 ? 0 : 1;
    EXPECT_CALL(filter_callbacks_, addDecodedData(_, false))
        .Times(decoded_data_calls);
    EXPECT_CALL(filter_callbacks_.downstream_callbacks_, clearRouteCache()).Times(0);
    auto res = filter_->decodeHeaders(headers_, true);
    EXPECT_EQ(Http::FilterHeadersStatus::Continue, res);
    EXPECT_EQ(val, config_->stats().request_header_transformations_.value());
  }

  void transformsOnHeadersAndClearCache(
      TransformationFilterTest::ConfigType configType, unsigned int val) {
    initFilterWithBodyTemplate(configType, "solo");
    route_config_.set_clear_route_cache(true);
    initFilter(); // Re-load config.

    EXPECT_CALL(filter_callbacks_, addDecodedData(_, false)).Times(1);
    EXPECT_CALL(filter_callbacks_.downstream_callbacks_, clearRouteCache()).Times(1);
    auto res = filter_->decodeHeaders(headers_, true);
    EXPECT_EQ(Http::FilterHeadersStatus::Continue, res);
    EXPECT_EQ(val, config_->stats().request_header_transformations_.value());
  }

  void transformsResponseOnHeaders(unsigned int val) {

    Http::TestResponseHeaderMapImpl response_headers{
        {"content-type", "test"},
        {":method", "GET"},
        {":authority", "www.solo.io"},
        {":path", "/path"}};
    route_config_.mutable_response_transformation()
        ->mutable_transformation_template()
        ->mutable_body()
        ->set_text("solo");

    initFilter();

    filter_->decodeHeaders(headers_, true);
    EXPECT_CALL(encoder_filter_callbacks_, addEncodedData(_, false)).Times(1);
    auto res = filter_->encodeHeaders(response_headers, true);
    EXPECT_EQ(Http::FilterHeadersStatus::Continue, res);
    EXPECT_EQ(val, config_->stats().response_header_transformations_.value());
  }

  void happyPathWithBody(TransformationFilterTest::ConfigType configType,
                         unsigned int val) {
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

  void
  happyPathWithBodyPassthrough(TransformationFilterTest::ConfigType configType,
                               unsigned int val) {
    initFilterWithBodyPassthrough(configType);

    auto resheaders = filter_->decodeHeaders(headers_, false);
    EXPECT_EQ(Http::FilterHeadersStatus::Continue, resheaders);

    Buffer::OwnedImpl downstream_body("{\"a\":\"b\"}");
    auto res = filter_->decodeData(downstream_body, true);
    EXPECT_EQ(Http::FilterDataStatus::Continue, res);
    EXPECT_EQ(0U, config_->stats().request_body_transformations_.value());
    EXPECT_EQ(val, config_->stats().request_header_transformations_.value());
  }

  Http::TestRequestHeaderMapImpl headers_{{"content-type", "test"},
                                          {":method", "GET"},
                                          {":authority", "www.solo.io"},
                                          {":path", "/path"}};

  NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_;
  NiceMock<Http::MockStreamEncoderFilterCallbacks> encoder_filter_callbacks_;

  std::unique_ptr<TransformationFilter> filter_;
  RouteTransformationConfigProto route_config_;
  TransformationConfigProto listener_config_;
  envoy::api::v2::filter::http::TransformationRule transformation_rule_;
  FilterConfigSharedPtr config_;
  RouteFilterConfigConstSharedPtr route_config_wrapper_;

  bool null_route_config_ = false;

  const std::string get_method_matcher_ = R"EOF(
    prefix: /
  )EOF";

  const std::string path_header_matcher_ = R"EOF(
    prefix: /path
  )EOF";

  const std::string invalid_header_matcher_ = R"EOF(
    prefix: /path-2
  )EOF";
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

TEST_F(TransformationFilterTest, SkipTransformWithInvalidHeaderMatcher) {
  null_route_config_ = true;
  addMatchersToListenerFilter(invalid_header_matcher_);
  transformsOnHeaders(TransformationFilterTest::ConfigType::Listener, 0U);
}

TEST_F(TransformationFilterTest, EnableTransformWithHeaderMatcher) {
  null_route_config_ = true;
  addMatchersToListenerFilter(path_header_matcher_);
  transformsOnHeaders(TransformationFilterTest::ConfigType::Listener, 1U);
}

TEST_F(TransformationFilterTest, IgnoreHeaderMatcherWithRouteConfig) {
  addMatchersToListenerFilter(invalid_header_matcher_);
  transformsOnHeadersAndClearCache(TransformationFilterTest::ConfigType::Both,
                                   1U);
}

TEST_F(TransformationFilterTest, TransformsOnHeadersAndClearCache) {
  transformsOnHeadersAndClearCache(TransformationFilterTest::ConfigType::Both,
                                   1U);
  transformsOnHeadersAndClearCache(TransformationFilterTest::ConfigType::Route,
                                   2U);
  transformsOnHeadersAndClearCache(
      TransformationFilterTest::ConfigType::Listener, 3U);
}

TEST_F(TransformationFilterTest, TransformsResponseOnHeaders) {
  transformsResponseOnHeaders(1U);
  transformsResponseOnHeaders(2U);
  transformsResponseOnHeaders(3U);
}

TEST_F(TransformationFilterTest, TransformsResponseOnHeadersNoHost) {
  Http::TestResponseHeaderMapImpl response_headers{
      {"content-type", "test"}, {":method", "GET"}, {":path", "/path"}};
  headers_.remove(":authority");
  route_config_.mutable_response_transformation()
      ->mutable_transformation_template()
      ->mutable_body()
      ->set_text("solo");

  initFilter();

  // no encode headers to simulate local reply error.
  EXPECT_CALL(encoder_filter_callbacks_, addEncodedData(_, false)).Times(0);
  auto res = filter_->encodeHeaders(response_headers, true);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, res);
  EXPECT_EQ(0U, config_->stats().response_header_transformations_.value());
}

TEST_F(TransformationFilterTest, TransformLocalResponse) {
  Http::TestRequestHeaderMapImpl request_headers{
      {"content-type", "test"}, {":method", "GET"}, {":path", "/"}};
  Http::TestResponseHeaderMapImpl response_headers{{"content-type", "test"},
                                                   {":status", "429"}};
  const std::string match_string = R"EOF(
  transformations:
  - response_match:
      match:
        headers:
        - name: ":status"
          exact_match: "200"
      response_transformation:
        transformation_template:
          passthrough: {}
          headers:
            ":status": {text: "401"}
  - response_match:
      match:
        headers:
        - name: ":status"
          exact_match: "429"
      response_transformation:
        transformation_template:
          passthrough: {}
          headers:
            ":status": {text: "400"}
  )EOF";
  TestUtility::loadFromYaml(match_string, route_config_);

  initFilter();

  auto res = filter_->decodeHeaders(request_headers, true);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, res);
  res = filter_->encodeHeaders(response_headers, true);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, res);
  EXPECT_EQ(response_headers.get_(":status"), "400");
}

TEST_F(TransformationFilterTest, StagedFilterResponseConfig) {
  Http::TestRequestHeaderMapImpl request_headers{
      {"content-type", "test"}, {":method", "GET"}, {":path", "/"}};
  Http::TestResponseHeaderMapImpl response_headers{{"content-type", "test"},
                                                   {":status", "200"}};
  listener_config_.set_stage(1);
  const std::string match_string = R"EOF(
  transformations:
  - stage: 0
    response_match:
      match:
        headers:
        - name: ":status"
          exact_match: "200"
      response_transformation:
        transformation_template:
          passthrough: {}
          headers:
            "x-foo": {text: "stage0"}
  - stage: 1
    response_match:
      match:
        headers:
        - name: ":status"
          exact_match: "200"
      response_transformation:
        transformation_template:
          passthrough: {}
          headers:
            "x-foo": {text: "stage1"}
  )EOF";
  TestUtility::loadFromYaml(match_string, route_config_);

  initFilter();

  auto res = filter_->decodeHeaders(request_headers, true);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, res);
  res = filter_->encodeHeaders(response_headers, true);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, res);
  EXPECT_EQ(response_headers.get_("x-foo"), "stage1");
}

TEST_F(TransformationFilterTest, StagedFilterRequestConfig) {
  Http::TestRequestHeaderMapImpl request_headers{
      {"content-type", "test"}, {":method", "GET"}, {":path", "/"}};
  listener_config_.set_stage(1);
  const std::string match_string = R"EOF(
  transformations:
  - stage: 0
    request_match:
      match:
        prefix: /
      request_transformation:
        transformation_template:
          passthrough: {}
          headers:
            "x-foo": {text: "stage0"}
  - stage: 1
    request_match:
      match:
        prefix: /
      request_transformation:
        transformation_template:
          passthrough: {}
          headers:
            "x-foo": {text: "stage1"}
  )EOF";
  TestUtility::loadFromYaml(match_string, route_config_);

  initFilter();

  auto res = filter_->decodeHeaders(request_headers, true);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, res);
  EXPECT_EQ(request_headers.get_("x-foo"), "stage1");
}

TEST_F(TransformationFilterTest, RequestMatchInOrder) {
  Http::TestRequestHeaderMapImpl request_headers{
      {"content-type", "test"}, {":method", "GET"}, {":path", "/foo"}};
  const std::string match_string = R"EOF(
  transformations:
  - request_match:
      match:
        prefix: /foo
      request_transformation:
        transformation_template:
          passthrough: {}
          headers:
            "x-foo": {text: "foo"}
  - request_match:
      match:
        prefix: /
      request_transformation:
        transformation_template:
          passthrough: {}
          headers:
            "x-foo": {text: "notfoo"}
  )EOF";
  TestUtility::loadFromYaml(match_string, route_config_);

  initFilter();

  auto res = filter_->decodeHeaders(request_headers, true);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, res);
  EXPECT_EQ(request_headers.get_("x-foo"), "foo");
}

TEST_F(TransformationFilterTest, RequestMissingMatchWins) {
  Http::TestRequestHeaderMapImpl request_headers{
      {"content-type", "test"}, {":method", "GET"}, {":path", "/foo"}};
  const std::string match_string = R"EOF(
  transformations:
  - request_match:
      request_transformation:
        transformation_template:
          passthrough: {}
          headers:
            "x-foo": {text: "foo"}
  - request_match:
      match:
        prefix: /
      request_transformation:
        transformation_template:
          passthrough: {}
          headers:
            "x-foo": {text: "notfoo"}
  )EOF";
  TestUtility::loadFromYaml(match_string, route_config_);

  initFilter();

  auto res = filter_->decodeHeaders(request_headers, true);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, res);
  EXPECT_EQ(request_headers.get_("x-foo"), "foo");
}

TEST_F(TransformationFilterTest, ResponseMatchesResponseCode) {
  Http::TestRequestHeaderMapImpl request_headers{
      {"content-type", "test"}, {":method", "GET"}, {":path", "/"}};
  Http::TestResponseHeaderMapImpl response_headers{{"content-type", "test"},
                                                   {":status", "429"}};
  encoder_filter_callbacks_.stream_info_.response_code_details_ = "ratelimit";
  const std::string match_string = R"EOF(
  transformations:
  - response_match:
      match:
        response_code_details:
          exact: "auth"
      response_transformation:
        transformation_template:
          passthrough: {}
          headers:
            ":status": {text: "401"}
  - response_match:
      match:
        response_code_details:
          exact: "ratelimit"
      response_transformation:
        transformation_template:
          passthrough: {}
          headers:
            ":status": {text: "400"}
  )EOF";
  TestUtility::loadFromYaml(match_string, route_config_);

  initFilter();

  auto res = filter_->decodeHeaders(request_headers, true);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, res);
  res = filter_->encodeHeaders(response_headers, true);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, res);
  EXPECT_EQ(response_headers.get_(":status"), "400");
}

TEST_F(TransformationFilterTest, ErrorOnBadTemplate) {
  initFilterWithBodyTemplate(TransformationFilterTest::ConfigType::Both,
                             "{{nonexistentvar}}");

  std::string status;
  EXPECT_CALL(filter_callbacks_, encodeHeaders_(_, _))
      .WillOnce(Invoke([&](Http::ResponseHeaderMap &headers, bool) {
        status = std::string(headers.Status()->value().getStringView());
      }));

  auto res = filter_->decodeHeaders(headers_, true);
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, res);
  EXPECT_EQ("400", status);
  EXPECT_EQ(1U, config_->stats().request_error_.value());
}

TEST_F(TransformationFilterTest, ErrorOnInvalidJsonBody) {
  initFilterWithBodyTemplate(TransformationFilterTest::ConfigType::Both,
                             "solo");

  auto resheaders = filter_->decodeHeaders(headers_, false);
  ASSERT_EQ(Http::FilterHeadersStatus::StopIteration, resheaders);

  std::string status;
  EXPECT_CALL(filter_callbacks_, encodeHeaders_(_, _))
      .WillOnce(Invoke([&](Http::ResponseHeaderMap &headers, bool) {
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
  happyPathWithBodyPassthrough(TransformationFilterTest::ConfigType::Both, 1U);
  happyPathWithBodyPassthrough(TransformationFilterTest::ConfigType::Route, 2U);
  happyPathWithBodyPassthrough(TransformationFilterTest::ConfigType::Listener,
                               3U);
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

TEST_F(TransformationFilterTest, HappyPathOnStreamComplete) {
  initOnStreamCompleteTransformHeader();

  Http::TestResponseHeaderMapImpl response_headers{
    {"content-type", "test"},
    {":method", "GET"},
    {":authority", "www.solo.io"},
    {":path", "/path"}};

  // Create response and request headers
  auto res = filter_->decodeHeaders(headers_, true);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, res);
  res = filter_->encodeHeaders(response_headers, true);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, res);

  // Header should not be added yet
  EXPECT_EQ(EMPTY_STRING, headers_.get_("added-header"));
  EXPECT_EQ(EMPTY_STRING, response_headers.get_("added-header"));

  filter_->onStreamComplete();
  // Header is added to response headers as part of onStreamComplete
  EXPECT_EQ("added-value", response_headers.get_("added-header"));
  EXPECT_EQ(EMPTY_STRING, headers_.get_("added-header"));
}

TEST_F(TransformationFilterTest, WithoutResponseHeaderOnStreamComplete) {
  initOnStreamCompleteTransformHeader();

  // There are only request headers
  auto res = filter_->decodeHeaders(headers_, true);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, res);
  EXPECT_EQ(EMPTY_STRING, headers_.get_("added-header"));

  // onStreamComplete should be successful (no errors) despite
  // response_headers being a nullptr (since it wasn't encoded)
  EXPECT_NO_THROW(filter_->onStreamComplete());
  EXPECT_EQ(0U, config_->stats().on_stream_complete_error_.value());
}

TEST_F(TransformationFilterTest, ErroredOnStreamComplete) {
  initOnStreamCompleteTransformHeader();

  Http::TestResponseHeaderMapImpl response_headers{
    {"content-type", "test"},
    {":method", "GET"},
    {":authority", "www.solo.io"},
    {":path", "/path"}};

  // Create response and request headers
  auto res = filter_->decodeHeaders(headers_, true);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, res);
  res = filter_->encodeHeaders(response_headers, true);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, res);

  // Raise an arbitrary error during a call that is triggeres
  // during the Inja header transformer
  ON_CALL(encoder_filter_callbacks_, clusterInfo())
        .WillByDefault(Throw(std::runtime_error("arbitrary error")));

  // Verify that error is caught and stat is incremented
  EXPECT_NO_THROW(filter_->onStreamComplete());
  EXPECT_EQ(1U, config_->stats().on_stream_complete_error_.value());

}

TEST_F(TransformationFilterTest, EncodeStopIterationOnFilterDestroy) {
  initFilterWithHeadersBody(TransformationFilterTest::ConfigType::Both);
  filter_->onDestroy();
  Http::TestResponseHeaderMapImpl response_headers{};
  auto ehResult = filter_->encodeHeaders(response_headers, false);
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, ehResult);

  Buffer::OwnedImpl buf{};

  auto edResult = filter_->encodeData(buf, false);
  EXPECT_EQ(Http::FilterDataStatus::StopIterationNoBuffer, edResult);

    Http::TestResponseTrailerMapImpl response_trailers{};
  auto etResult = filter_->encodeTrailers(response_trailers);
  EXPECT_EQ(Http::FilterTrailersStatus::StopIteration, etResult);

}

TEST_F(TransformationFilterTest, AutoWebsocketPassthrough) {
  listener_config_.set_auto_websocket_passthrough(true);
  envoy::api::v2::filter::http::InjaTemplate header_value;
  header_value.set_text("bar");
  (*route_config_.mutable_response_transformation()
        ->mutable_transformation_template()
        ->mutable_headers())["foo"] = header_value;
  initFilterWithHeadersBody(TransformationFilterTest::ConfigType::Both);

  Http::TestRequestHeaderMapImpl request_headers{{":method", "GET"},
                                                 {":authority", "www.solo.io"},
                                                 {":path", "/ws-endpoint"},
                                                 {"upgrade", "websocket"},
                                                 {"connection", "Upgrade"},
                                                 {"sec-webSocket-key", "dGhlIHNhbXBsZSBub25jZQ=="},
                                                 {"sec-webSocket-version", "13"},
                                                 {"sec-webSocket-protocol", "chat, superchat"},
                                                 {"origin", "https://www.solo.io"}};

  Http::TestResponseHeaderMapImpl response_headers{{":status", "101"},
                                                   {"upgrade", "websocket"},
                                                   {"connection", "Upgrade"},
                                                   {"sec-webSocket-accept", "s3pPLMBiTxaQ9kYGzzhZRbK+xOo="},
                                                   {"sec-webSocket-protocol", "chat"}};
  // On websocket request, we don't buffer, so we should get Continue in all
  // the encode/decodeHeader and encode/decodeData even end_stream is false
  auto headerResult = filter_->decodeHeaders(request_headers, false);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, headerResult);

  Buffer::OwnedImpl dummy{};
  auto dataResult = filter_->decodeData(dummy, false);
  EXPECT_EQ(Http::FilterDataStatus::Continue, dataResult);

  headerResult = filter_->encodeHeaders(response_headers, false);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, headerResult);

  dataResult = filter_->encodeData(dummy, false);
  EXPECT_EQ(Http::FilterDataStatus::Continue, dataResult);

  // for non-websocket request, even to the same end point, we should buffer by default
  request_headers.remove("upgrade");

  headerResult = filter_->decodeHeaders(request_headers, false);
  EXPECT_NE(Http::FilterHeadersStatus::Continue, headerResult);

  dataResult = filter_->decodeData(dummy, false);
  EXPECT_NE(Http::FilterDataStatus::Continue, dataResult);

  // even the response_headers here is 101 but it doesn't matter, the websocket 
  // detection is base on the request header only. 
  headerResult = filter_->encodeHeaders(response_headers, false);
  EXPECT_NE(Http::FilterHeadersStatus::Continue, headerResult);

  dataResult = filter_->encodeData(dummy, false);
  EXPECT_NE(Http::FilterDataStatus::Continue, dataResult);
}

struct MockLogSink : Envoy::Logger::SinkDelegate {
  MockLogSink(Logger::DelegatingLogSinkSharedPtr log_sink) : SinkDelegate(log_sink) { setDelegate(); }
  ~MockLogSink() override { restoreDelegate(); }

  MOCK_METHOD(void, log, (absl::string_view, const spdlog::details::log_msg&));
  MOCK_METHOD(void, logWithStableName,
              (absl::string_view, absl::string_view, absl::string_view, absl::string_view));
  void flush() override {}
};

TEST_F(TransformationFilterTest, LogWithLogDetailsTransformationLevel) {
  Envoy::Logger::Registry::setLogLevel(spdlog::level::debug);

  MockLogSink sink(Envoy::Logger::Registry::getSink());

  EXPECT_CALL(sink, log(_, _)).WillRepeatedly(Invoke([](auto msg, __attribute__((unused)) auto& log) {
    EXPECT_THAT(msg, testing::HasSubstr("[debug]"));
    EXPECT_THAT(msg, testing::AnyOf(testing::HasSubstr("headers before transformation: "),
                                    testing::HasSubstr("body before transformation: "),
                                    testing::HasSubstr("headers after transformation: "),
                                    testing::HasSubstr("body after transformation: ")));
  }));

  // set log_request_response_info on transformation config
  transformation_rule_.mutable_route_transformations()
                ->mutable_request_transformation()->mutable_log_request_response_info()->set_value(true);
  // confirm that log_request_response_info on listener config is ignored when transformation-level setting is true
  listener_config_.set_log_request_response_info(false);
  happyPathWithBody(TransformationFilterTest::ConfigType::Route, 1U);
}

TEST_F(TransformationFilterTest, LogWithLogDetailsListenerLevel) {
  Envoy::Logger::Registry::setLogLevel(spdlog::level::debug);

  MockLogSink sink(Envoy::Logger::Registry::getSink());

  EXPECT_CALL(sink, log(_, _)).WillRepeatedly(Invoke([](auto msg, __attribute__((unused)) auto& log) {
    EXPECT_THAT(msg, testing::HasSubstr("[debug]"));
    EXPECT_THAT(msg, testing::AnyOf(testing::HasSubstr("headers before transformation: "),
                                    testing::HasSubstr("body before transformation: "),
                                    testing::HasSubstr("headers after transformation: "),
                                    testing::HasSubstr("body after transformation: ")));
  }));

  // set log_request_response_info on listener config
  listener_config_.set_log_request_response_info(true);

  happyPathWithBody(TransformationFilterTest::ConfigType::Route, 1U);
}

TEST_F(TransformationFilterTest, LogWithLogDetailsListenerLevelTransformationLevelFalse) {
  Envoy::Logger::Registry::setLogLevel(spdlog::level::debug);

  MockLogSink sink(Envoy::Logger::Registry::getSink());

  EXPECT_CALL(sink, log(_, _)).WillOnce(Invoke([](auto msg, __attribute__((unused)) auto& log) {
    EXPECT_THAT(msg, testing::HasSubstr("test"));
  }));

  // set log_request_response_info on listener config
  listener_config_.set_log_request_response_info(true);
  // confirm that log_request_response_info on transformation-level setting is respected when listener config is true
  transformation_rule_.mutable_route_transformations()
            ->mutable_request_transformation()->mutable_log_request_response_info()->set_value(false);
  happyPathWithBody(TransformationFilterTest::ConfigType::Route, 1U);

  // if processing the transformation generates a log, then this line will cause the "WillOnce" expectation above to fail
  sink.log("test", spdlog::details::log_msg());
}

TEST_F(TransformationFilterTest, NoLogWhenLogWithDetailsDisabled) {
    Envoy::Logger::Registry::setLogLevel(spdlog::level::debug);

  MockLogSink sink(Envoy::Logger::Registry::getSink());

  EXPECT_CALL(sink, log(_, _)).WillOnce(Invoke([](auto msg, __attribute__((unused)) auto& log) {
    EXPECT_THAT(msg, testing::HasSubstr("test"));
  }));

  // set log_request_response_info false on both transformation and listener config
  listener_config_.set_log_request_response_info(false);
  transformation_rule_.mutable_route_transformations()
            ->mutable_request_transformation()->mutable_log_request_response_info()->set_value(false);
  happyPathWithBody(TransformationFilterTest::ConfigType::Route, 1U);

  // if processing the transformation generates a log, then this line will cause the "WillOnce" expectation above to fail
  sink.log("test", spdlog::details::log_msg());
}

TEST_F(TransformationFilterTest, MatcherOnListenerConfig) {
  Http::TestRequestHeaderMapImpl request_headers{
      {"content-type", "test"}, {":method", "GET"}, {":path", "/pathprefix"}};
  null_route_config_ = true;
  const std::string match_string = R"EOF(
  matcher:
    matcher_list:
      matchers:
      - predicate:
          single_predicate:
            input:
              name: envoy.matching.inputs.request_headers
              typed_config:
                "@type": type.googleapis.com/envoy.type.matcher.v3.HttpRequestHeaderMatchInput
                header_name: ":path"
            value_match:
              prefix: "/pathprefix"
        on_match:
          action:
            name: action
            typed_config:
              "@type": type.googleapis.com/envoy.api.v2.filter.http.TransformationRule.Transformations
              request_transformation:
                transformation_template:
                  passthrough: {}
                  headers:
                    "x-foo": {text: "matcher"}
  )EOF";
  TestUtility::loadFromYaml(match_string, listener_config_);

  initFilter();

  auto res = filter_->decodeHeaders(request_headers, true);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, res);
  EXPECT_EQ(request_headers.get_("x-foo"), "matcher");
}

TEST_F(TransformationFilterTest, MatcherOnRouteConfig) {
  Http::TestRequestHeaderMapImpl request_headers{
      {"content-type", "test"}, {":method", "GET"}, {":path", "/pathprefix"}};

    const std::string match_string = R"EOF(
  transformations:
  - matcher:
      matcher_list:
        matchers:
        - predicate:
            single_predicate:
              input:
                name: envoy.matching.inputs.request_headers
                typed_config:
                  "@type": type.googleapis.com/envoy.type.matcher.v3.HttpRequestHeaderMatchInput
                  header_name: ":path"
              value_match:
                prefix: "/pathprefix"
          on_match:
            action:
              name: action
              typed_config:
                "@type": type.googleapis.com/envoy.api.v2.filter.http.TransformationRule.Transformations
                request_transformation:
                  transformation_template:
                    passthrough: {}
                    headers:
                      "x-foo": {text: "matcher"}
  )EOF";
  TestUtility::loadFromYaml(match_string, route_config_);

  initFilter();

  auto res = filter_->decodeHeaders(request_headers, true);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, res);
  EXPECT_EQ(request_headers.get_("x-foo"), "matcher");
}

TEST_F(TransformationFilterTest, MatcherOnRouteConfigTypedStruct) {
  Http::TestRequestHeaderMapImpl request_headers{
      {"content-type", "test"}, {":method", "GET"}, {":path", "/pathprefix"}};

    const std::string match_string = R"EOF(
  transformations:
  - matcher:
      matcher_list:
        matchers:
        - predicate:
            single_predicate:
              input:
                name: envoy.matching.inputs.request_headers
                typed_config:
                  "@type": "type.googleapis.com/xds.type.v3.TypedStruct"
                  type_url: "type.googleapis.com/envoy.type.matcher.v3.HttpRequestHeaderMatchInput"
                  value:
                    header_name: ":path"
              value_match:
                prefix: "/pathprefix"
          on_match:
            action:
              name: action
              typed_config:
                "@type": "type.googleapis.com/xds.type.v3.TypedStruct"
                type_url: "type.googleapis.com/envoy.api.v2.filter.http.TransformationRule.Transformations"
                value:
                  request_transformation:
                    transformation_template:
                      passthrough: {}
                      headers:
                        "x-foo": {text: "matcher"}
  )EOF";
  TestUtility::loadFromYaml(match_string, route_config_);

  initFilter();

  auto res = filter_->decodeHeaders(request_headers, true);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, res);
  EXPECT_EQ(request_headers.get_("x-foo"), "matcher");
}



} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
