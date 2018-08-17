#include "extensions/filters/http/aws_lambda/aws_lambda_filter.h"
#include "extensions/filters/http/aws_lambda/aws_lambda_filter_config_factory.h"
#include "extensions/filters/http/aws_lambda_well_known_names.h"

#include "test/mocks/common.h"
#include "test/mocks/server/mocks.h"
#include "test/mocks/upstream/mocks.h"
#include "test/test_common/utility.h"

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
namespace Http {

using Server::Configuration::AWSLambdaFilterConfigFactory;

class AWSLambdaFilterTest : public testing::Test {
public:
  AWSLambdaFilterTest() {}

protected:
  void SetUp() override {

    routeconfig_.set_name("func");
    routeconfig_.set_qualifier("v1");
    routeconfig_.set_async(false);

    setup_func();

    envoy::config::filter::http::aws_lambda::v2::AWSLambdaProtocolExtension
        protoextconfig;
    protoextconfig.set_host("lambda.us-east-1.amazonaws.com");
    protoextconfig.set_region("us-east-1");
    protoextconfig.set_access_key("access key");
    protoextconfig.set_secret_key("secret key");

    ON_CALL(
        *factory_context_.cluster_manager_.thread_local_cluster_.cluster_.info_,
        extensionProtocolOptions(
            Config::AWSLambdaHttpFilterNames::get().AWS_LAMBDA))
        .WillByDefault(Return(
            std::make_shared<AWSLambdaProtocolExtensionConfig>(protoextconfig)));

    filter_ =
        std::make_unique<AWSLambdaFilter>(factory_context_.cluster_manager_);
    filter_->setDecoderFilterCallbacks(filter_callbacks_);
  }

  void setup_func() {

    filter_route_config_.reset(new AWSLambdaRouteConfig(routeconfig_));

    ON_CALL(filter_callbacks_.route_->route_entry_,
            perFilterConfig(Config::AWSLambdaHttpFilterNames::get().AWS_LAMBDA))
        .WillByDefault(Return(filter_route_config_.get()));
  }

  NiceMock<MockStreamDecoderFilterCallbacks> filter_callbacks_;
  NiceMock<Server::Configuration::MockFactoryContext> factory_context_;

  std::unique_ptr<AWSLambdaFilter> filter_;
  envoy::config::filter::http::aws_lambda::v2::AWSLambdaPerRoute routeconfig_;
  std::unique_ptr<AWSLambdaRouteConfig> filter_route_config_;
};

// see:
// https://docs.aws.amazon.com/AmazonS3/latest/API/sig-v4-header-based-auth.html
TEST_F(AWSLambdaFilterTest, SingsOnHeadersEndStream) {

  TestHeaderMapImpl headers{{":method", "GET"},
                            {":authority", "www.solo.io"},
                            {":path", "/getsomething"}};
  EXPECT_EQ(FilterHeadersStatus::Continue,
            filter_->decodeHeaders(headers, true));

  // Check aws headers.
  EXPECT_TRUE(headers.has("Authorization"));
}

TEST_F(AWSLambdaFilterTest, SingsOnDataEndStream) {

  TestHeaderMapImpl headers{{":method", "GET"},
                            {":authority", "www.solo.io"},
                            {":path", "/getsomething"}};

  EXPECT_EQ(FilterHeadersStatus::StopIteration,
            filter_->decodeHeaders(headers, false));
  EXPECT_FALSE(headers.has("Authorization"));
  Buffer::OwnedImpl data("data");

  EXPECT_EQ(FilterDataStatus::Continue, filter_->decodeData(data, true));

  EXPECT_TRUE(headers.has("Authorization"));
}

// see: https://docs.aws.amazon.com/lambda/latest/dg/API_Invoke.html
TEST_F(AWSLambdaFilterTest, CorrectFuncCalled) {
  TestHeaderMapImpl headers{{":method", "GET"},
                            {":authority", "www.solo.io"},
                            {":path", "/getsomething"}};

  EXPECT_EQ(FilterHeadersStatus::Continue,
            filter_->decodeHeaders(headers, true));

  EXPECT_EQ("/2015-03-31/functions/" + routeconfig_.name() +
                "/invocations?Qualifier=" + routeconfig_.qualifier(),
            headers.get_(":path"));
}

// see: https://docs.aws.amazon.com/lambda/latest/dg/API_Invoke.html
TEST_F(AWSLambdaFilterTest, FuncWithEmptyQualifierCalled) {
  routeconfig_.clear_qualifier();
  setup_func();

  TestHeaderMapImpl headers{{":method", "GET"},
                            {":authority", "www.solo.io"},
                            {":path", "/getsomething"}};

  EXPECT_EQ(FilterHeadersStatus::Continue,
            filter_->decodeHeaders(headers, true));

  EXPECT_EQ("/2015-03-31/functions/" + routeconfig_.name() + "/invocations",
            headers.get_(":path"));
}

TEST_F(AWSLambdaFilterTest, AsyncCalled) {
  TestHeaderMapImpl headers{{":method", "GET"},
                            {":authority", "www.solo.io"},
                            {":path", "/getsomething"}};
  routeconfig_.set_async(true);
  setup_func();

  EXPECT_EQ(FilterHeadersStatus::Continue,
            filter_->decodeHeaders(headers, true));
  EXPECT_EQ("Event", headers.get_("x-amz-invocation-type"));
}

TEST_F(AWSLambdaFilterTest, SyncCalled) {
  TestHeaderMapImpl headers{{":method", "GET"},
                            {":authority", "www.solo.io"},
                            {":path", "/getsomething"}};

  routeconfig_.set_async(false);
  setup_func();

  EXPECT_EQ(FilterHeadersStatus::Continue,
            filter_->decodeHeaders(headers, true));
  EXPECT_EQ("RequestResponse", headers.get_("x-amz-invocation-type"));
}

TEST_F(AWSLambdaFilterTest, SignOnTrailedEndStream) {
  TestHeaderMapImpl headers{{":method", "GET"},
                            {":authority", "www.solo.io"},
                            {":path", "/getsomething"}};

  EXPECT_EQ(FilterHeadersStatus::StopIteration,
            filter_->decodeHeaders(headers, false));
  Buffer::OwnedImpl data("data");

  EXPECT_EQ(FilterDataStatus::StopIterationAndBuffer,
            filter_->decodeData(data, false));
  EXPECT_FALSE(headers.has("Authorization"));

  TestHeaderMapImpl trailers;
  EXPECT_EQ(FilterTrailersStatus::Continue, filter_->decodeTrailers(trailers));

  EXPECT_TRUE(headers.has("Authorization"));
}

TEST_F(AWSLambdaFilterTest, InvalidFunction) {
  // invalid function
  EXPECT_CALL(
      filter_callbacks_.route_->route_entry_,
      perFilterConfig(Config::AWSLambdaHttpFilterNames::get().AWS_LAMBDA))
      .WillRepeatedly(Return(nullptr));

  TestHeaderMapImpl headers{{":method", "GET"},
                            {":authority", "www.solo.io"},
                            {":path", "/getsomething"}};
  EXPECT_EQ(FilterHeadersStatus::StopIteration,
            filter_->decodeHeaders(headers, true));
}

} // namespace Http
} // namespace Envoy
