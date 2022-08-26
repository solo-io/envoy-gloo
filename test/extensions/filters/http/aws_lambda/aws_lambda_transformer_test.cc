#include "source/extensions/filters/http/aws_lambda/aws_authenticator.h"
#include "source/extensions/filters/http/aws_lambda/aws_lambda_filter.h"
#include "source/extensions/filters/http/aws_lambda/aws_lambda_filter_config_factory.h"

#include "source/extensions/filters/http/transformation/transformation_filter_config.h"
#include "test/extensions/filters/http/aws_lambda/test_transformer.h"

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
    if (credentials_ == nullptr) {
      callbacks->onFailure(CredentialsFailureStatus::Network);
    } else {
      callbacks->onSuccess(credentials_);
    }
    return nullptr;
  }

  CredentialsConstSharedPtr credentials_;
  mutable bool called_{};

  bool propagateOriginalRouting() const override{
    return propagate_original_routing_;
  }
  bool propagate_original_routing_;
};

class AWSLambdaTransformerTest : public testing::Test {
public:
  AWSLambdaTransformerTest() {}

protected:
  void SetUp() override { setupRoute(); }

  const std::string TRANSFORMATION_YAML =
  R"EOF(
  name: io.solo.transformer.api_gateway_test_transformer
  typed_config:
    "@type": type.googleapis.com/envoy.test.extensions.transformation.ApiGatewayTestTransformer
  )EOF";

  void setupApiGatewayRequestTransformation() {
    TestUtility::loadFromYaml(TRANSFORMATION_YAML, *routeconfig_.mutable_request_transformer_config());
  }

  void setupApiGatewayResponseTransformation() {
    TestUtility::loadFromYaml(TRANSFORMATION_YAML, *routeconfig_.mutable_transformer_config());
  }

  void setupRoute(bool unwrapAsApiGateway = false, bool wrapAsApiGateway = false) {
    factory_context_.cluster_manager_.initializeClusters({"fake_cluster"}, {});
    factory_context_.cluster_manager_.initializeThreadLocalClusters({"fake_cluster"});

    routeconfig_.set_name("func");
    routeconfig_.set_qualifier("v1");
    routeconfig_.set_async(false);

    if (unwrapAsApiGateway) {
      setupApiGatewayResponseTransformation();
    }

    if (wrapAsApiGateway) {
      setupApiGatewayRequestTransformation();
    }

    setup_func();

    envoy::config::filter::http::aws_lambda::v2::AWSLambdaProtocolExtension
        protoextconfig;
    protoextconfig.set_host("lambda.us-east-1.amazonaws.com");
    protoextconfig.set_region("us-east-1");
    filter_config_ = std::make_shared<AWSLambdaConfigTestImpl>();

    filter_config_->credentials_ =
        std::make_shared<Envoy::Extensions::Common::Aws::Credentials>(
            "access key", "secret key");
    
    filter_config_->propagate_original_routing_=false;

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

    ON_CALL(*filter_callbacks_.route_,
            mostSpecificPerFilterConfig(SoloHttpFilterNames::get().AwsLambda))
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
    filter_->encodeHeaders(response_headers, false);
    return response_headers;
  }

  NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_;
  NiceMock<Http::MockStreamEncoderFilterCallbacks> filter_encode_callbacks_;
  NiceMock<Server::Configuration::MockFactoryContext> factory_context_;
  NiceMock<Server::Configuration::MockServerFactoryContext> server_factory_context_;

  std::unique_ptr<AWSLambdaFilter> filter_;
  envoy::config::filter::http::aws_lambda::v2::AWSLambdaPerRoute routeconfig_;
  std::unique_ptr<AWSLambdaRouteConfig> filter_route_config_;
  std::shared_ptr<AWSLambdaConfigTestImpl> filter_config_;
};

TEST_F(AWSLambdaTransformerTest, SignsOnHeadersEndStream) {

  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"}};
  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            filter_->decodeHeaders(headers, true));

  // Check aws headers.
  EXPECT_TRUE(headers.has("Authorization"));
}

TEST_F(AWSLambdaTransformerTest, TestConfigureRequestTransformer){
  setupRoute(false, true);
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"}};

  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->decodeHeaders(headers, false));

  Buffer::OwnedImpl data("hello");

  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(data, true));

  // Confirm that the body is transformed to the hardcoded string.
  EXPECT_EQ(data.toString(), "test body from fake transformer");
}

TEST_F(AWSLambdaTransformerTest, TestConfigureResponseTransformer){
  setupRoute(true, false);
  auto request_headers = setup_encode();

  Buffer::OwnedImpl buf{};
  auto on_buf_mod = [&buf](std::function<void(Buffer::Instance&)> cb){cb(buf);};
  EXPECT_CALL(filter_encode_callbacks_, encodingBuffer).WillOnce(Return(&buf));
  EXPECT_CALL(filter_encode_callbacks_, modifyEncodingBuffer)
      .WillOnce(Invoke(on_buf_mod));

  auto edResult = filter_->encodeData(buf, true);

  EXPECT_EQ(Http::FilterDataStatus::Continue, edResult);
  EXPECT_STREQ("test body from fake transformer", buf.toString().c_str());
}

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
