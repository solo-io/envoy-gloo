#include "source/extensions/filters/http/aws_lambda/aws_authenticator.h"
#include "source/extensions/filters/http/aws_lambda/aws_lambda_filter.h"
#include "source/extensions/filters/http/aws_lambda/aws_lambda_filter_config_factory.h"

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
  StsConnectionPool::Context *getCredentials(
      SharedAWSLambdaProtocolExtensionConfig,
      StsConnectionPool::Context::Callbacks *callbacks) const override {
    called_ = true;
    return getCreds(callbacks);
  }

  CredentialsConstSharedPtr credentials_;
  mutable bool called_{};

  bool propagateOriginalRouting() const override{
    return propagate_original_routing_;
  }

  MOCK_METHOD(StsConnectionPool::Context *, getCreds,  
                   (StsConnectionPool::Context::Callbacks *callbacks), (const));

  bool propagate_original_routing_;
};

class StsContextStub : public StsConnectionPool::Context {
      StsConnectionPool::Context::Callbacks *callbacks() const override{
        return nullptr;
      };

    /**
     * Cancels the request if it is in flight
     */
    void cancel() override{
      return;
    }
};

class AWSLambdaFilterTest : public testing::Test {
public:
  AWSLambdaFilterTest() {}

protected:
  void SetUp() override { setupRoute(); }

  void setupRoute(bool sessionToken = false, bool noCredentials = false,
                bool persistOriginalHeaders = false, bool unwrapAsAlb = false,
                bool unmanagedCredentials = false) {
    factory_context_.cluster_manager_.initializeClusters({"fake_cluster"}, {});
    factory_context_.cluster_manager_.initializeThreadLocalClusters({"fake_cluster"});

    routeconfig_.set_name("func");
    routeconfig_.set_qualifier("v1");
    routeconfig_.set_async(false);
    routeconfig_.set_unwrap_as_alb(unwrapAsAlb);

    setup_func();

    envoy::config::filter::http::aws_lambda::v2::AWSLambdaProtocolExtension
        protoextconfig;
    protoextconfig.set_host("lambda.us-east-1.amazonaws.com");
    protoextconfig.set_region("us-east-1");
    filter_config_ = std::make_shared<testing::NiceMock<
                                      AWSLambdaConfigTestImpl>>();

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
    if (!unmanagedCredentials){
      ON_CALL(*filter_config_, getCreds).WillByDefault(
                        [this](StsConnectionPool::Context::Callbacks *callbacks){
          if (filter_config_->credentials_ == nullptr) {
              callbacks->onFailure(CredentialsFailureStatus::Network);
          } else {
            callbacks->onSuccess(filter_config_->credentials_);
          }
          return nullptr;
        }
      );
    }
    
    filter_config_->propagate_original_routing_=persistOriginalHeaders;

    ON_CALL(
        *factory_context_.cluster_manager_.thread_local_cluster_.cluster_.info_,
        extensionProtocolOptions(SoloHttpFilterNames::get().AwsLambda))
        .WillByDefault(
            Return(std::make_shared<AWSLambdaProtocolExtensionConfig>(
                protoextconfig)));


    filter_ = std::make_unique<AWSLambdaFilter>(
        factory_context_.cluster_manager_, factory_context_.api_,
        filter_config_);
    filter_->setDecoderFilterCallbacks(filter_callbacks_);

    
  }

  void setup_func() {

    filter_route_config_.reset(new AWSLambdaRouteConfig(routeconfig_, server_factory_context_));

    ON_CALL(filter_callbacks_,
            mostSpecificPerFilterConfig())
        .WillByDefault(Return(filter_route_config_.get()));
  }

  Http::TestResponseHeaderMapImpl setup_encode(){
    // Run normal operations for side-effects e.g.: setting function_on_route_
    Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                     {":authority", "www.solo.io"}, {":path", "/getsomething"}};
    filter_->decodeHeaders(headers, true);
    Http::TestResponseHeaderMapImpl response_headers{
                    {":method", "GET"}, {":status", "200"}, {":path", "/path"}};
    filter_->setEncoderFilterCallbacks(filter_encode_callbacks_);
    filter_->encodeHeaders(response_headers, true);
    return response_headers;
  }

  NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_;
  NiceMock<Http::MockStreamEncoderFilterCallbacks> filter_encode_callbacks_;
  NiceMock<Server::Configuration::MockFactoryContext> factory_context_;
  NiceMock<Server::Configuration::MockServerFactoryContext> server_factory_context_;

  std::unique_ptr<AWSLambdaFilter> filter_;
  envoy::config::filter::http::aws_lambda::v2::AWSLambdaPerRoute routeconfig_;
  std::unique_ptr<AWSLambdaRouteConfig> filter_route_config_;
  std::shared_ptr<NiceMock<AWSLambdaConfigTestImpl>> filter_config_;
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
  auto header(headers.get(AwsAuthenticatorConsts::get().SecurityTokenHeader));
  ASSERT_EQ(header.size(), 1);
  EXPECT_EQ(header[0]->value(), "session token");
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
  EXPECT_EQ(
      headers.get(AwsAuthenticatorConsts::get().SecurityTokenHeader)[0]->value(),
      "session token");
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

TEST_F(AWSLambdaFilterTest, PropagateOriginalHeaders) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"}};

  setupRoute(false, false, true);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, true));
  EXPECT_EQ("/getsomething", headers.get_("x-envoy-original-path"));
}
TEST_F(AWSLambdaFilterTest, DontPropagateOriginalHeaders) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"}};


  setupRoute();
  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            filter_->decodeHeaders(headers, true));
  EXPECT_EQ("", headers.get_("x-envoy-original-path"));
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

TEST_F(AWSLambdaFilterTest, SignsDataSetByPreviousFilters) {
  // there are cases where even if our filter is waiting on decode headers
  // decode data can still happen. We need to verify that bodysha is still
  // handled appropriately.

  // we will manage credentials callbacks ourselves for this test
  setupRoute(false, false, false, false, true);

  Http::TestRequestHeaderMapImpl headers_1{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"}};
  Buffer::OwnedImpl data_1("data");
  Http::TestRequestTrailerMapImpl trailers_1;


  StsConnectionPool::Context::Callbacks *callbackReference;
  StsContextStub fakeContext;
  EXPECT_CALL(*filter_config_, getCreds).WillRepeatedly(
     [&](StsConnectionPool::Context::Callbacks *callbacks) ->
                                         StsConnectionPool::Context*{
        callbackReference = callbacks;
        // context is used for evaluating if we are in async
        // only other use is cancel so just use stub.
        return &fakeContext;
     }
  );

  filter_->decodeHeaders(headers_1, false);
  callbackReference->onSuccess(filter_config_->credentials_);
  filter_->decodeData(data_1, false);
  filter_->decodeTrailers(trailers_1);
  auto auth1 = filter_->awsAuthenticator();
  auto hex_sha1 = auth1.getBodyHexSha();

  filter_ = std::make_unique<AWSLambdaFilter>(
      factory_context_.cluster_manager_, factory_context_.api_,
      filter_config_);
  filter_->setDecoderFilterCallbacks(filter_callbacks_);

  // Pointers are a thing so lets just restate the above values
  Http::TestRequestHeaderMapImpl headers_2{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"}};
  Buffer::OwnedImpl data_2("data");
  Http::TestRequestTrailerMapImpl trailers_2;

  // intentionally misorder our on success to simulate a long sts call.
  filter_->decodeHeaders(headers_2, false);
  filter_->decodeData(data_2, false);
  callbackReference->onSuccess(filter_config_->credentials_);
  filter_->decodeTrailers(trailers_2);
  auto auth2 = filter_->awsAuthenticator();
  auto hex_sha2 = auth2.getBodyHexSha();
  EXPECT_EQ(hex_sha1, hex_sha2);

  filter_ = std::make_unique<AWSLambdaFilter>(
      factory_context_.cluster_manager_, factory_context_.api_,
      filter_config_);
  filter_->setDecoderFilterCallbacks(filter_callbacks_);



  Http::TestRequestHeaderMapImpl headers_3{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"}};
  Buffer::OwnedImpl data_3("data");
  Http::TestRequestTrailerMapImpl trailers_3;

  // intentionally misorder our on success to simulate a long sts call 
  // where data is sent with end stream true
  filter_->decodeHeaders(headers_3, false);
  filter_->decodeData(data_3, true);
  callbackReference->onSuccess(filter_config_->credentials_);
  auto auth3 = filter_->awsAuthenticator();
  auto hex_sha3 = auth3.getBodyHexSha();

  EXPECT_EQ(hex_sha1, hex_sha3);
}

TEST_F(AWSLambdaFilterTest, InvalidFunction) {
  // invalid function
  EXPECT_CALL(filter_callbacks_,
              mostSpecificPerFilterConfig())
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
  ON_CALL(filter_callbacks_,
          mostSpecificPerFilterConfig())
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

TEST_F(AWSLambdaFilterTest, UpstreamErrorSetTo504) {
  setup_func();

  Http::TestResponseHeaderMapImpl response_headers{
    {"content-type", "test"},
    {":method", "GET"},
    {":authority", "www.solo.io"},
    {":status", "200"},
    {"x-amz-function-error", "fakerr"},
    {":path", "/path"}};
  auto res = filter_->encodeHeaders(response_headers, true);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, res);
  EXPECT_EQ(response_headers.getStatusValue(), "504");
  
}

TEST_F(AWSLambdaFilterTest, ALBDecodingBasic) {
  setupRoute(false, false, false, true);
  
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                     {":authority", "www.solo.io"}, {":path", "/getsomething"}};
  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
                filter_->decodeHeaders(headers, true));
  Http::TestResponseHeaderMapImpl response_headers{
                    {":method", "GET"}, {":status", "200"}, {":path", "/path"}};

  filter_->setEncoderFilterCallbacks(filter_encode_callbacks_);
  auto res = filter_->encodeHeaders(response_headers, true);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, res);
  

  Buffer::OwnedImpl buf{};
  // Based off the following split url.
  //https://docs.aws.amazon.com/elasticloadbalancing/latest/
  //                  application/lambda-functions.html#respond-to-load-balancer
  buf.add("{ \"isBase64Encoded\": false, \"statusCode\": 200,");
   

  auto edResult = filter_->encodeData(buf, false);
  buf.add(
   "\"statusDescription\": \"200 OK\","
    "\"headers\": {"
    "   \"Set-cookie\": \"cookies\", \"Content-Type\": \"application/json\""
    "},"
    "\"body\": \"Hello from Lambda (optional)\""
    "}");
  EXPECT_EQ(Http::FilterDataStatus::StopIterationAndBuffer, edResult);
  auto on_buf_mod = [&buf](std::function<void(Buffer::Instance&)> cb){cb(buf);};
  EXPECT_CALL(filter_encode_callbacks_, encodingBuffer).WillOnce(Return(&buf));
  EXPECT_CALL(filter_encode_callbacks_, modifyEncodingBuffer)
      .WillOnce(Invoke(on_buf_mod));

  auto edResult2 = filter_->encodeData(buf, false);
  EXPECT_EQ(Http::FilterDataStatus::StopIterationAndBuffer, edResult2);
  Http::TestResponseTrailerMapImpl response_trailers_;
  auto etResult = filter_->encodeTrailers(response_trailers_);
  EXPECT_EQ(Http::FilterTrailersStatus::Continue, etResult);
  EXPECT_STREQ("Hello from Lambda (optional)", buf.toString().c_str());
  EXPECT_EQ("200", response_headers.getStatusValue());
  ASSERT_NE(response_headers.ContentType(), nullptr);
  EXPECT_EQ("application/json", response_headers.getContentTypeValue());
  auto cookieHeader = response_headers.get(Http::LowerCaseString("set-cookie"));
  EXPECT_EQ("cookies", cookieHeader[0]->value().getStringView());
}

TEST_F(AWSLambdaFilterTest, ALBDecodingMultiValueHeaders) {
  setupRoute(false, false, false, true);
  auto response_headers = setup_encode();

  Buffer::OwnedImpl buf{};
  buf.add("{\"multiValueHeaders\": {"
      "\"Set-cookie\":"
      "[\"cookie-name=cookie-value;Domain=myweb.com;Secure;HttpOnly\","
      "\"cookie-name=cookie-value;Expires=May 8, 2019\"],"
      "\"Content-Type\": [\"application/json\"]"
      "},}");

  auto on_buf_mod = [&buf](std::function<void(Buffer::Instance&)> cb){cb(buf);};
  EXPECT_CALL(filter_encode_callbacks_, encodingBuffer).WillOnce(Return(&buf));
  EXPECT_CALL(filter_encode_callbacks_, modifyEncodingBuffer)
      .WillOnce(Invoke(on_buf_mod));

  auto edResult2 = filter_->encodeData(buf, true);
  EXPECT_EQ(Http::FilterDataStatus::Continue, edResult2);
  EXPECT_STREQ("", buf.toString().c_str());
  EXPECT_EQ("200", response_headers.getStatusValue());
  EXPECT_EQ("application/json", response_headers.getContentTypeValue());
  auto cookieHeader = response_headers.get(Http::LowerCaseString("set-cookie"));
  EXPECT_EQ("cookie-name=cookie-value;Domain=myweb.com;Secure;HttpOnly",
                                      cookieHeader[0]->value().getStringView());
  EXPECT_EQ("cookie-name=cookie-value;Expires=May 8, 2019", 
                                      cookieHeader[1]->value().getStringView());
}

TEST_F(AWSLambdaFilterTest, ALBDecodingBase64) {
  setupRoute(false, false, false, true);
  auto response_headers = setup_encode();

  Buffer::OwnedImpl buf{};
  buf.add("{ \"isBase64Encoded\": true, \"statusCode\": 201,"
            "\"body\": \"SGVsbG8gZnJvbSBMYW1iZGEgKG9wdGlvbmFsKQ==\"}");

  auto on_buf_mod = [&buf](std::function<void(Buffer::Instance&)> cb){cb(buf);};
  EXPECT_CALL(filter_encode_callbacks_, encodingBuffer).WillOnce(Return(&buf));
  EXPECT_CALL(filter_encode_callbacks_, modifyEncodingBuffer)
      .WillOnce(Invoke(on_buf_mod));


  auto edResult2 = filter_->encodeData(buf, true);
  EXPECT_EQ(Http::FilterDataStatus::Continue, edResult2);
  EXPECT_STREQ("Hello from Lambda (optional)", buf.toString().c_str());
  EXPECT_EQ("201", response_headers.getStatusValue());
}

TEST_F(AWSLambdaFilterTest, ALBDecodingInvalidTypes) {
  setupRoute(false, false, false, true);
  auto response_headers = setup_encode();

  Buffer::OwnedImpl buf{};
  buf.add("{ \"isBase64Encoded\": \"notabool\", \"statusCode\": 201,"
            "\"body\": \"else==\"}");

  auto on_buf_mod = [&buf](std::function<void(Buffer::Instance&)> cb){cb(buf);};

  EXPECT_CALL(filter_encode_callbacks_, encodingBuffer).WillOnce(Return(&buf));
  EXPECT_CALL(filter_encode_callbacks_, modifyEncodingBuffer)
      .WillOnce(Invoke(on_buf_mod));

  auto edResult2 = filter_->encodeData(buf, true);
  EXPECT_EQ(Http::FilterDataStatus::Continue, edResult2);
  EXPECT_STREQ("", buf.toString().c_str());
  EXPECT_EQ("500", response_headers.getStatusValue());
}

TEST_F(AWSLambdaFilterTest, ALBDecodingInvalidJSON) {
  setupRoute(false, false, false, true);
  auto response_headers = setup_encode();

  Buffer::OwnedImpl buf{};
  buf.add("{ \"isBase64Encoded\": floof, \"statusCode\": 201,"
            "\"body\": \"something\"}"
            "\"body\": \"else==\"}");

  auto on_buf_mod = [&buf](std::function<void(Buffer::Instance&)> cb){cb(buf);};

  EXPECT_CALL(filter_encode_callbacks_, encodingBuffer).WillOnce(Return(&buf));
  EXPECT_CALL(filter_encode_callbacks_, modifyEncodingBuffer)
      .WillOnce(Invoke(on_buf_mod));


  auto edResult2 = filter_->encodeData(buf, true);
  EXPECT_EQ(Http::FilterDataStatus::Continue, edResult2);
  EXPECT_STREQ("", buf.toString().c_str());
  EXPECT_EQ("500", response_headers.getStatusValue());
}

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
