#include "source/common/config/metadata.h"

#include "source/extensions/filters/http/solo_well_known_names.h"

#include "test/integration/http_integration.h"
#include "test/integration/integration.h"
#include "test/integration/utility.h"

#include "api/envoy/config/filter/http/aws_lambda/v2/aws_lambda.pb.validate.h"

namespace Envoy {

const std::string DEFAULT_LAMBDA_FILTER =
    R"EOF(
name: io.solo.aws_lambda
typed_config:
  "@type": type.googleapis.com/envoy.config.filter.http.aws_lambda.v2.AWSLambdaConfig
)EOF";

const std::string USE_CHAIN_LAMBDA_FILTER =
    R"EOF(
name: io.solo.aws_lambda
typed_config:
  "@type": type.googleapis.com/envoy.config.filter.http.aws_lambda.v2.AWSLambdaConfig
  use_default_credentials: true
)EOF";

const std::string USE_STS_LAMBDA_FILTER =
    R"EOF(
name: io.solo.aws_lambda
typed_config:
  "@type": type.googleapis.com/envoy.config.filter.http.aws_lambda.v2.AWSLambdaConfig
  service_account_credentials:
    cluster: cluster_0
    uri: https://foo.com
    region: us-east-1
    timeout: 1s
)EOF";

const std::string VALID_CHAINED_RESPONSE = R"(
  <AssumeRoleResponse xmlns="https://sts.amazonaws.com/doc/2011-06-15/">
  <AssumeRoleResult>
  <SourceIdentity>Alice</SourceIdentity>
    <AssumedRoleUser>
      <Arn>to_chain_role_arn</Arn>
    </AssumedRoleUser>
    <Credentials>
      <AccessKeyId>some_second_access_key</AccessKeyId>
      <SecretAccessKey>some_second_secret_key</SecretAccessKey>
      <SessionToken>some_session_token</SessionToken>
      <Expiration>3000-07-28T21:20:25Z</Expiration>
    </Credentials>
    <PackedPolicySize>1</PackedPolicySize>
  </AssumeRoleResult>
  <ResponseMetadata>
    <RequestId>unique01-uuid-or23-some-thingliketha</RequestId>
  </ResponseMetadata>
</AssumeRoleResponse>
)";

class AWSLambdaFilterIntegrationTest
    : public HttpIntegrationTest,
      public testing::TestWithParam<Network::Address::IpVersion> {
public:
  AWSLambdaFilterIntegrationTest()
      : HttpIntegrationTest(Http::CodecClient::Type::HTTP1, GetParam()) {}

  void TearDown() override {
    TestEnvironment::unsetEnvVar("AWS_ACCESS_KEY_ID");
    TestEnvironment::unsetEnvVar("AWS_SECRET_ACCESS_KEY");
  }

  /**
   * Initializer for an individual integration test.
   */
  void initialize() override {

    if (add_transform_) {
      // not sure why but checking the "authorization" in the test succeeds.
      // what i really want to test is that the filter that follows the aws filter gets the auth header
      // so i use transformation to copy the auth header to another header, and test that header instead.
      config_helper_.prependFilter(R"EOF(
name: io.solo.aws_lambda
typed_config:
  "@type": type.googleapis.com/envoy.api.v2.filter.http.FilterTransformations
  transformations:
  - match:
      prefix: /
    route_transformations:
      request_transformation:
        transformation_template:
          advanced_templates: false
          headers:
            x-authorization:
              text: '{{header("authorization")}}'
          passthrough: {}
)EOF");

    }
    switch (cred_mode_) {
      case CredMode::CHAIN:
      // set env vars for test
      TestEnvironment::setEnvVar("AWS_ACCESS_KEY_ID", "access key", 1);
      TestEnvironment::setEnvVar("AWS_SECRET_ACCESS_KEY", "access key", 1);
      TestEnvironment::setEnvVar("AWS_SESSION_TOKEN", "session token", 1);
      config_helper_.prependFilter(USE_CHAIN_LAMBDA_FILTER);
      break;
    case CredMode::STS:
      // set env vars for test
      TestEnvironment::setEnvVar("AWS_WEB_IDENTITY_TOKEN_FILE", TestEnvironment::runfilesPath("test/integration/fakejwt.txt", "envoy_gloo"), 1);
      TestEnvironment::setEnvVar("AWS_ROLE_ARN", "test", 1);
      config_helper_.prependFilter(USE_STS_LAMBDA_FILTER);
      break;
    case CredMode::DEFAULT:
      config_helper_.prependFilter(DEFAULT_LAMBDA_FILTER);
    }

    if (add_transform_) {
      config_helper_.prependFilter(R"EOF(
name: io.solo.aws_lambda
typed_config:
  "@type": type.googleapis.com/envoy.api.v2.filter.http.FilterTransformations
  transformations:
  - match:
      prefix: /
    route_transformations:
      request_transformation:
        transformation_template:
          advanced_templates: true
          extractors:
            ext1:
              header: :path
              regex: (.*)
              subgroup: 1
          headers:
            x-solo:
              text: solo.io
          body:
            text: abc {{extraction("ext1")}}
)EOF");

    }

    config_helper_.addConfigModifier([this](
                                         envoy::config::bootstrap::v3::Bootstrap
                                             &bootstrap) {
      envoy::config::filter::http::aws_lambda::v2::AWSLambdaProtocolExtension
          protoextconfig;
      protoextconfig.set_host("lambda.us-east-1.amazonaws.com");
      protoextconfig.set_region("us-east-1");
      if (cred_mode_ == CredMode::DEFAULT) {
        protoextconfig.set_access_key("access key");
        protoextconfig.set_secret_key("secret key");
        protoextconfig.set_session_token("session token");
      }
      ProtobufWkt::Struct functionstruct;

      auto &lambda_cluster =
          (*bootstrap.mutable_static_resources()->mutable_clusters(0));

      auto &cluster_any =
          (*lambda_cluster.mutable_typed_extension_protocol_options())
              [Extensions::HttpFilters::SoloHttpFilterNames::get().AwsLambda];
      cluster_any.PackFrom(protoextconfig);
    });

    config_helper_.addConfigModifier(
        [](envoy::extensions::filters::network::http_connection_manager::v3::
               HttpConnectionManager &hcm) {
          auto &mostSpecificPerFilterConfig = (*hcm.mutable_route_config()
                                        ->mutable_virtual_hosts(0)
                                        ->mutable_routes(0)
                                        ->mutable_typed_per_filter_config())
              [Extensions::HttpFilters::SoloHttpFilterNames::get().AwsLambda];

          envoy::config::filter::http::aws_lambda::v2::AWSLambdaPerRoute
              proto_config;
          proto_config.set_name("FunctionName");
          proto_config.set_qualifier("v1");
          mostSpecificPerFilterConfig.PackFrom(proto_config);
        });

    HttpIntegrationTest::initialize();

    codec_client_ =
        makeHttpConnection(makeClientConnection((lookupPort("http"))));
  }

  enum class CredMode{DEFAULT, CHAIN, STS};
  CredMode cred_mode_{};
  bool add_transform_{};
};

INSTANTIATE_TEST_SUITE_P(
    IpVersions, AWSLambdaFilterIntegrationTest,
    testing::ValuesIn(TestEnvironment::getIpVersionsForTest()));

TEST_P(AWSLambdaFilterIntegrationTest, TestWithConfig) {
  initialize();
  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "POST"}, {":authority", "www.solo.io"}, {":path", "/"}};

  sendRequestAndWaitForResponse(request_headers, 10, default_response_headers_,
                                10);

  EXPECT_NE(0, upstream_request_->headers()
                   .get(Http::LowerCaseString("authorization"))[0]
                   ->value()
                   .size());
}

TEST_P(AWSLambdaFilterIntegrationTest, TestWithChain) {
  cred_mode_ = CredMode::CHAIN;
  initialize();
  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "POST"}, {":authority", "www.solo.io"}, {":path", "/"}};

  sendRequestAndWaitForResponse(request_headers, 10, default_response_headers_,
                                10);

  EXPECT_NE(0, upstream_request_->headers()
                   .get(Http::LowerCaseString("authorization"))[0]
                   ->value()
                   .size());
  EXPECT_EQ(1UL,
            test_server_->gauge("http.config_test.aws_lambda.current_state")
                ->value());
  EXPECT_EQ(1UL,
            test_server_->counter("http.config_test.aws_lambda.creds_rotated")
                ->value());
  EXPECT_EQ(0UL,
            test_server_->counter("http.config_test.aws_lambda.fetch_failed")
                ->value());
}

TEST_P(AWSLambdaFilterIntegrationTest, TestWithSTS) {
  cred_mode_ = CredMode::STS;
  add_transform_ = true;
  initialize();
  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "GET"}, {":authority", "www.solo.io"}, {":path", "/"}};

  IntegrationStreamDecoderPtr response = codec_client_->makeHeaderOnlyRequest(request_headers);

  auto timeout = TestUtility::DefaultTimeout;

  // first request is sts request; return sts response.
  waitForNextUpstreamRequest(0, timeout);
  upstream_request_->encodeHeaders(default_response_headers_, false);
  upstream_request_->encodeData(VALID_CHAINED_RESPONSE, true);

  // second upstream request is the "lambda" request.
  waitForNextUpstreamRequest(0, timeout);

  // make sure we have a body (i.e. make sure transformation filter worked).
  std::string body = upstream_request_->body().toString();
  EXPECT_EQ(body, "abc /");

  // make sure that the transformation filter after the lambda was called and observed the authorization header:
  // ** THIS IS THE MANIFESTATION OF THE BUG **
  const auto& auth_header = upstream_request_->headers()
                   .get(Http::LowerCaseString("x-authorization"))[0]
                   ->value();
  EXPECT_NE(0, auth_header.size());

  // wrap up the test nicely:
  upstream_request_->encodeHeaders(default_response_headers_, false);
  upstream_request_->encodeData(10, true);

  // Wait for the response to be read by the codec client.
  RELEASE_ASSERT(response->waitForEndStream(timeout), "unexpected timeout");
}


} // namespace Envoy
