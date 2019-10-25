#include "extensions/filters/http/aws_signer/filter.h"
#include "extensions/filters/http/aws_signer/filter_config_factory.h"

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
namespace Extensions {
namespace HttpFilters {
namespace AwsSigner {

class AwsSignerFilterTest : public testing::Test {
public:
  AwsSignerFilterTest() {}

protected:
  void SetUp() override {

    routeconfig_.set_name("func");
    routeconfig_.set_qualifier("v1");
    routeconfig_.set_async(false);

    setup_func();

    envoy::config::filter::http::aws_signer::v2::AWSSignerProtocolExtension
        protoextconfig;
    protoextconfig.set_host("lambda.us-east-1.amazonaws.com");
    protoextconfig.set_region("us-east-1");
    protoextconfig.set_access_key("access key");
    protoextconfig.set_secret_key("secret key");

    ON_CALL(
        *factory_context_.cluster_manager_.thread_local_cluster_.cluster_.info_,
        extensionProtocolOptions(SoloHttpFilterNames::get().AwsSigner))
        .WillByDefault(
            Return(std::make_shared<AwsSignerProtocolExtensionConfig>(
                protoextconfig)));

    filter_ = std::make_unique<AwsSignerFilter>(
        factory_context_.cluster_manager_,
        factory_context_.dispatcher().timeSource());
    filter_->setDecoderFilterCallbacks(filter_callbacks_);
  }

  void setup_func() {

    filter_route_config_.reset(new AwsSignerRouteConfig(routeconfig_));

    ON_CALL(filter_callbacks_.route_->route_entry_,
            perFilterConfig(SoloHttpFilterNames::get().AwsSigner))
        .WillByDefault(Return(filter_route_config_.get()));
  }

  NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_;
  NiceMock<Server::Configuration::MockFactoryContext> factory_context_;

  std::unique_ptr<AwsSignerFilter> filter_;
  envoy::config::filter::http::aws_lambda::v2::AwsSignerPerRoute routeconfig_;
  std::unique_ptr<AwsSignerRouteConfig> filter_route_config_;
};

// see:
// https://docs.aws.amazon.com/AmazonS3/latest/API/sig-v4-header-based-auth.html
TEST_F(AwsSignerFilterTest, SingsOnHeadersEndStream) {

  Http::TestHeaderMapImpl headers{{":method", "GET"},
                                  {":authority", "www.solo.io"},
                                  {":path", "/getsomething"}};
  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            filter_->decodeHeaders(headers, true));

  // Check aws headers.
  EXPECT_TRUE(headers.has("Authorization"));
}

TEST_F(AwsSignerFilterTest, SingsOnDataEndStream) {

  Http::TestHeaderMapImpl headers{{":method", "GET"},
                                  {":authority", "www.solo.io"},
                                  {":path", "/getsomething"}};

  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->decodeHeaders(headers, false));
  EXPECT_FALSE(headers.has("Authorization"));
  Buffer::OwnedImpl data("data");

  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(data, true));

  EXPECT_TRUE(headers.has("Authorization"));
}

// see: https://docs.aws.amazon.com/lambda/latest/dg/API_Invoke.html
TEST_F(AwsSignerFilterTest, CorrectFuncCalled) {
  Http::TestHeaderMapImpl headers{{":method", "GET"},
                                  {":authority", "www.solo.io"},
                                  {":path", "/getsomething"}};

  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            filter_->decodeHeaders(headers, true));

  EXPECT_EQ("/2015-03-31/functions/" + routeconfig_.name() +
                "/invocations?Qualifier=" + routeconfig_.qualifier(),
            headers.get_(":path"));
}

// see: https://docs.aws.amazon.com/lambda/latest/dg/API_Invoke.html
TEST_F(AwsSignerFilterTest, FuncWithEmptyQualifierCalled) {
  routeconfig_.clear_qualifier();
  setup_func();

  Http::TestHeaderMapImpl headers{{":method", "GET"},
                                  {":authority", "www.solo.io"},
                                  {":path", "/getsomething"}};

  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            filter_->decodeHeaders(headers, true));

  EXPECT_EQ("/2015-03-31/functions/" + routeconfig_.name() + "/invocations",
            headers.get_(":path"));
}

TEST_F(AwsSignerFilterTest, AsyncCalled) {
  Http::TestHeaderMapImpl headers{{":method", "GET"},
                                  {":authority", "www.solo.io"},
                                  {":path", "/getsomething"}};
  routeconfig_.set_async(true);
  setup_func();

  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            filter_->decodeHeaders(headers, true));
  EXPECT_EQ("Event", headers.get_("x-amz-invocation-type"));
}

TEST_F(AwsSignerFilterTest, SyncCalled) {
  Http::TestHeaderMapImpl headers{{":method", "GET"},
                                  {":authority", "www.solo.io"},
                                  {":path", "/getsomething"}};

  routeconfig_.set_async(false);
  setup_func();

  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            filter_->decodeHeaders(headers, true));
  EXPECT_EQ("RequestResponse", headers.get_("x-amz-invocation-type"));
}

TEST_F(AwsSignerFilterTest, SignOnTrailedEndStream) {
  Http::TestHeaderMapImpl headers{{":method", "GET"},
                                  {":authority", "www.solo.io"},
                                  {":path", "/getsomething"}};

  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->decodeHeaders(headers, false));
  Buffer::OwnedImpl data("data");

  EXPECT_EQ(Http::FilterDataStatus::StopIterationAndBuffer,
            filter_->decodeData(data, false));
  EXPECT_FALSE(headers.has("Authorization"));

  Http::TestHeaderMapImpl trailers;
  EXPECT_EQ(Http::FilterTrailersStatus::Continue,
            filter_->decodeTrailers(trailers));

  EXPECT_TRUE(headers.has("Authorization"));
}

TEST_F(AwsSignerFilterTest, InvalidFunction) {
  // invalid function
  EXPECT_CALL(filter_callbacks_.route_->route_entry_,
              perFilterConfig(SoloHttpFilterNames::get().AwsSigner))
      .WillRepeatedly(Return(nullptr));

  Http::TestHeaderMapImpl headers{{":method", "GET"},
                                  {":authority", "www.solo.io"},
                                  {":path", "/getsomething"}};
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->decodeHeaders(headers, true));
}

TEST_F(AwsSignerFilterTest, EmptyBodyGetsOverriden) {
  routeconfig_.mutable_empty_body_override()->set_value("{}");
  setup_func();

  Http::TestHeaderMapImpl headers{{":method", "GET"},
                                  {":authority", "www.solo.io"},
                                  {":path", "/getsomething"}};

  EXPECT_CALL(filter_callbacks_, addDecodedData(_, _))
      .Times(1)
      .WillOnce(Invoke([](Buffer::Instance &data, bool) {
        EXPECT_EQ(data.toString(), "{}");
      }));

  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            filter_->decodeHeaders(headers, true));
  EXPECT_EQ(headers.get_("content-type"), "application/json");
  EXPECT_EQ(headers.get_("content-length"), "2");
}

TEST_F(AwsSignerFilterTest, NonEmptyBodyDoesNotGetsOverriden) {
  routeconfig_.mutable_empty_body_override()->set_value("{}");
  setup_func();

  Http::TestHeaderMapImpl headers{{":method", "POST"},
                                  {":authority", "www.solo.io"},
                                  {":path", "/getsomething"}};

  // Expect the no added data
  EXPECT_CALL(filter_callbacks_, addDecodedData(_, _)).Times(0);

  filter_->decodeHeaders(headers, false);

  Buffer::OwnedImpl body("body");
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(body, true));
}

TEST_F(AwsSignerFilterTest, EmptyDecodedDataBufferGetsOverriden) {
  routeconfig_.mutable_empty_body_override()->set_value("{}");
  setup_func();

  Http::TestHeaderMapImpl headers{{":method", "POST"},
                                  {":authority", "www.solo.io"},
                                  {":path", "/getsomething"}};

  filter_->decodeHeaders(headers, false);

  EXPECT_CALL(filter_callbacks_, addDecodedData(_, _))
      .Times(1)
      .WillOnce(Invoke([](Buffer::Instance &data, bool) {
        EXPECT_EQ(data.toString(), "{}");
      }));

  Buffer::OwnedImpl body("");
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(body, true));
}

TEST_F(AwsSignerFilterTest, EmptyBodyWithTrailersGetsOverriden) {
  routeconfig_.mutable_empty_body_override()->set_value("{}");
  setup_func();

  Http::TestHeaderMapImpl headers{{":method", "POST"},
                                  {":authority", "www.solo.io"},
                                  {":path", "/getsomething"}};

  filter_->decodeHeaders(headers, false);

  EXPECT_CALL(filter_callbacks_, addDecodedData(_, _))
      .Times(1)
      .WillOnce(Invoke([](Buffer::Instance &data, bool) {
        EXPECT_EQ(data.toString(), "{}");
      }));

  filter_->decodeTrailers(headers);
}

} // namespace AwsSigner
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
