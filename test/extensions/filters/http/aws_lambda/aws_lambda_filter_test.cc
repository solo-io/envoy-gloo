#include "extensions/filters/http/aws_lambda/aws_lambda_filter.h"
#include "extensions/filters/http/aws_lambda/aws_lambda_filter_config_factory.h"

#include "extensions/filters/http/aws_lambda/aws_authenticator.h"

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
namespace AwsLambda {

class AWSLambdaConfigTestImpl : public AWSLambdaConfig {
public:
  ContextSharedPtr getCredentials(SharedAWSLambdaProtocolExtensionConfig, StsCredentialsProvider::Callbacks* callbacks) const override {
    called_ = true;
    if (credentials_ == nullptr) {
      callbacks->onSuccess(std::make_shared<const Envoy::Extensions::Common::Aws::Credentials>());  
    } else {
      callbacks->onSuccess(credentials_);
    }
    return nullptr;
  }

  CredentialsConstSharedPtr credentials_;
  mutable bool called_{};
};

class AWSLambdaFilterTest : public testing::Test {
public:
  AWSLambdaFilterTest() {}

protected:
  void SetUp() override { setupRoute(); }

  void setupRoute(bool sessionToken = false, bool noCredentials = false) {

    routeconfig_.set_name("func");
    routeconfig_.set_qualifier("v1");
    routeconfig_.set_async(false);

    setup_func();

    envoy::config::filter::http::aws_lambda::v2::AWSLambdaProtocolExtension
        protoextconfig;
    protoextconfig.set_host("lambda.us-east-1.amazonaws.com");
    protoextconfig.set_region("us-east-1");
    filter_config_ = std::make_shared<AWSLambdaConfigTestImpl>();


    if (!noCredentials) {
      if (sessionToken) {
        filter_config_->credentials_ =
            std::make_shared<Envoy::Extensions::Common::Aws::Credentials>(
                "access key", "secret key", "session token");
      } else {
        filter_config_->credentials_ =
            std::make_shared<Envoy::Extensions::Common::Aws::Credentials>(
                "access key", "secret key");
      }
    }

    ON_CALL(
        *factory_context_.cluster_manager_.thread_local_cluster_.cluster_.info_,
        extensionProtocolOptions(SoloHttpFilterNames::get().AwsLambda))
        .WillByDefault(
            Return(std::make_shared<AWSLambdaProtocolExtensionConfig>(
                protoextconfig)));

    filter_ = std::make_unique<AWSLambdaFilter>(
        factory_context_.cluster_manager_,
        factory_context_.api_, filter_config_);
    filter_->setDecoderFilterCallbacks(filter_callbacks_);
  }

  void setup_func() {

    filter_route_config_.reset(new AWSLambdaRouteConfig(routeconfig_));

    ON_CALL(filter_callbacks_.route_->route_entry_,
            perFilterConfig(SoloHttpFilterNames::get().AwsLambda))
        .WillByDefault(Return(filter_route_config_.get()));
  }

  NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_;
  NiceMock<Server::Configuration::MockFactoryContext> factory_context_;

  std::unique_ptr<AWSLambdaFilter> filter_;
  envoy::config::filter::http::aws_lambda::v2::AWSLambdaPerRoute routeconfig_;
  std::unique_ptr<AWSLambdaRouteConfig> filter_route_config_;
  std::shared_ptr<AWSLambdaConfigTestImpl> filter_config_;
};

// see:
// https://docs.aws.amazon.com/AmazonS3/latest/API/sig-v4-header-based-auth.html
TEST_F(AWSLambdaFilterTest, SignsOnHeadersEndStream) {

  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"}};
  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            filter_->decodeHeaders(headers, true));

  // Check aws headers.
  EXPECT_TRUE(headers.has("Authorization"));
}

TEST_F(AWSLambdaFilterTest, SignsOnHeadersEndStreamWithToken) {
  setupRoute(true);
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"}};
  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            filter_->decodeHeaders(headers, true));

  // Check aws headers.
  EXPECT_TRUE(headers.has("Authorization"));
  EXPECT_EQ(headers.get(AwsAuthenticatorConsts::get().SecurityTokenHeader)->value(), "session token");
}

TEST_F(AWSLambdaFilterTest, SignsOnHeadersEndStreamWithConfig) {
  setupRoute();

  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"}};
  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            filter_->decodeHeaders(headers, true));

  EXPECT_TRUE(filter_config_->called_);
  // Check aws headers.
  EXPECT_TRUE(headers.has("Authorization"));
}

TEST_F(AWSLambdaFilterTest, SignsOnHeadersEndStreamWithConfigWithToken) {
  setupRoute(true);

  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"}};
  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            filter_->decodeHeaders(headers, true));

  EXPECT_TRUE(filter_config_->called_);
  // Check aws headers.
  EXPECT_TRUE(headers.has("Authorization"));
  EXPECT_EQ(headers.get(AwsAuthenticatorConsts::get().SecurityTokenHeader)->value(), "session token");
}

TEST_F(AWSLambdaFilterTest, SignsOnHeadersEndStreamWithBadConfig) {
  setupRoute();
  filter_config_->credentials_ =
      std::make_shared<Envoy::Extensions::Common::Aws::Credentials>(
          "access key");

  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"}};
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->decodeHeaders(headers, true));

  // Check no aws headers.
  EXPECT_TRUE(filter_config_->called_);
  EXPECT_FALSE(headers.has("Authorization"));
}

TEST_F(AWSLambdaFilterTest, SignsOnDataEndStream) {

  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
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
TEST_F(AWSLambdaFilterTest, CorrectFuncCalled) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"}};

  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            filter_->decodeHeaders(headers, true));

  EXPECT_EQ("/2015-03-31/functions/" + routeconfig_.name() +
                "/invocations?Qualifier=" + routeconfig_.qualifier(),
            headers.get_(":path"));
}

// see: https://docs.aws.amazon.com/lambda/latest/dg/API_Invoke.html
TEST_F(AWSLambdaFilterTest, FuncWithEmptyQualifierCalled) {
  routeconfig_.clear_qualifier();
  setup_func();

  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"}};

  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            filter_->decodeHeaders(headers, true));

  EXPECT_EQ("/2015-03-31/functions/" + routeconfig_.name() + "/invocations",
            headers.get_(":path"));
}

TEST_F(AWSLambdaFilterTest, AsyncCalled) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"}};
  routeconfig_.set_async(true);
  setup_func();

  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            filter_->decodeHeaders(headers, true));
  EXPECT_EQ("Event", headers.get_("x-amz-invocation-type"));
}

TEST_F(AWSLambdaFilterTest, SyncCalled) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"}};

  routeconfig_.set_async(false);
  setup_func();

  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            filter_->decodeHeaders(headers, true));
  EXPECT_EQ("RequestResponse", headers.get_("x-amz-invocation-type"));
}

TEST_F(AWSLambdaFilterTest, SignOnTrailedEndStream) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"}};

  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->decodeHeaders(headers, false));
  Buffer::OwnedImpl data("data");

  EXPECT_EQ(Http::FilterDataStatus::StopIterationAndBuffer,
            filter_->decodeData(data, false));
  EXPECT_FALSE(headers.has("Authorization"));

  Http::TestRequestTrailerMapImpl trailers;
  EXPECT_EQ(Http::FilterTrailersStatus::Continue,
            filter_->decodeTrailers(trailers));

  EXPECT_TRUE(headers.has("Authorization"));
}

TEST_F(AWSLambdaFilterTest, InvalidFunction) {
  // invalid function
  EXPECT_CALL(filter_callbacks_.route_->route_entry_,
              perFilterConfig(SoloHttpFilterNames::get().AwsLambda))
      .WillRepeatedly(Return(nullptr));

  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"}};
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->decodeHeaders(headers, true));
}

TEST_F(AWSLambdaFilterTest, EmptyBodyGetsOverriden) {
  routeconfig_.mutable_empty_body_override()->set_value("{}");
  setup_func();

  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
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

TEST_F(AWSLambdaFilterTest, NonEmptyBodyDoesNotGetsOverriden) {
  routeconfig_.mutable_empty_body_override()->set_value("{}");
  setup_func();

  Http::TestRequestHeaderMapImpl headers{{":method", "POST"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"}};

  // Expect the no added data
  EXPECT_CALL(filter_callbacks_, addDecodedData(_, _)).Times(0);

  filter_->decodeHeaders(headers, false);

  Buffer::OwnedImpl body("body");
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(body, true));
}

TEST_F(AWSLambdaFilterTest, EmptyDecodedDataBufferGetsOverriden) {
  routeconfig_.mutable_empty_body_override()->set_value("{}");
  setup_func();

  Http::TestRequestHeaderMapImpl headers{{":method", "POST"},
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

TEST_F(AWSLambdaFilterTest, EmptyBodyWithTrailersGetsOverriden) {
  routeconfig_.mutable_empty_body_override()->set_value("{}");
  setup_func();

  Http::TestRequestHeaderMapImpl headers{{":method", "POST"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"}};

  filter_->decodeHeaders(headers, false);

  EXPECT_CALL(filter_callbacks_, addDecodedData(_, _))
      .Times(1)
      .WillOnce(Invoke([](Buffer::Instance &data, bool) {
        EXPECT_EQ(data.toString(), "{}");
      }));

  Http::TestRequestTrailerMapImpl trailers{{":method", "POST"},
                                           {":authority", "www.solo.io"},
                                           {":path", "/getsomething"}};

  filter_->decodeTrailers(trailers);
}

TEST_F(AWSLambdaFilterTest, NoFunctionOnRoute) {
  ON_CALL(filter_callbacks_.route_->route_entry_,
          perFilterConfig(SoloHttpFilterNames::get().AwsLambda))
      .WillByDefault(Return(nullptr));

  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"}};

  EXPECT_CALL(filter_callbacks_,
              sendLocalReply(Http::Code::InternalServerError, _, _, _, _))
      .Times(1);

  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->decodeHeaders(headers, true));
}

TEST_F(AWSLambdaFilterTest, NoCredsAvailable) {
  setupRoute(false, true);

  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"}};

  EXPECT_CALL(filter_callbacks_,
              sendLocalReply(Http::Code::InternalServerError, _, _, _, _))
      .Times(1);

  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->decodeHeaders(headers, true));
}

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
