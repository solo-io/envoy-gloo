#include "common/config/metadata.h"

#include "extensions/filters/http/solo_well_known_names.h"

#include "test/integration/http_integration.h"
#include "test/integration/integration.h"
#include "test/integration/utility.h"

#include "api/envoy/config/filter/http/aws_lambda/v2/aws_lambda.pb.validate.h"

namespace Envoy {

const std::string DEFAULT_LAMBDA_FILTER =
    R"EOF(
name: io.solo.aws_lambda
)EOF";

const std::string USE_CHAIN_LAMBDA_FILTER =
    R"EOF(
name: io.solo.aws_lambda
typed_config:
  "@type": type.googleapis.com/envoy.config.filter.http.aws_lambda.v2.AWSLambdaConfig
  use_default_credentials: true
)EOF";

class AWSLambdaFilterIntegrationTest
    : public HttpIntegrationTest,
      public testing::TestWithParam<Network::Address::IpVersion> {
public:
  AWSLambdaFilterIntegrationTest()
      : HttpIntegrationTest(Http::CodecClient::Type::HTTP1, GetParam(),
                            realTime()) {}

  void TearDown() override {
    TestEnvironment::unsetEnvVar("AWS_ACCESS_KEY_ID");
    TestEnvironment::unsetEnvVar("AWS_SECRET_ACCESS_KEY");
  }

  /**
   * Initializer for an individual integration test.
   */
  void initialize() override {
    if (use_chain_) {
      // set env vars for test
      TestEnvironment::setEnvVar("AWS_ACCESS_KEY_ID", "access key", 1);
      TestEnvironment::setEnvVar("AWS_SECRET_ACCESS_KEY", "access key", 1);
      TestEnvironment::setEnvVar("AWS_SESSION_TOKEN", "session token", 1);
      config_helper_.addFilter(USE_CHAIN_LAMBDA_FILTER);
    } else {
      config_helper_.addFilter(DEFAULT_LAMBDA_FILTER);
    }

    config_helper_.addConfigModifier([this](
                                         envoy::config::bootstrap::v3::Bootstrap
                                             &bootstrap) {
      envoy::config::filter::http::aws_lambda::v2::AWSLambdaProtocolExtension
          protoextconfig;
      protoextconfig.set_host("lambda.us-east-1.amazonaws.com");
      protoextconfig.set_region("us-east-1");
      if (!use_chain_) {
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
          auto &perFilterConfig = (*hcm.mutable_route_config()
                                        ->mutable_virtual_hosts(0)
                                        ->mutable_routes(0)
                                        ->mutable_typed_per_filter_config())
              [Extensions::HttpFilters::SoloHttpFilterNames::get().AwsLambda];

          envoy::config::filter::http::aws_lambda::v2::AWSLambdaPerRoute
              proto_config;
          proto_config.set_name("FunctionName");
          proto_config.set_qualifier("v1");
          perFilterConfig.PackFrom(proto_config);
        });

    HttpIntegrationTest::initialize();

    codec_client_ =
        makeHttpConnection(makeClientConnection((lookupPort("http"))));
  }

  bool use_chain_{};
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
                   .get(Http::LowerCaseString("authorization"))
                   ->value()
                   .size());
}
TEST_P(AWSLambdaFilterIntegrationTest, TestWithChain) {
  use_chain_ = true;
  initialize();
  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "POST"}, {":authority", "www.solo.io"}, {":path", "/"}};

  sendRequestAndWaitForResponse(request_headers, 10, default_response_headers_,
                                10);

  EXPECT_NE(0, upstream_request_->headers()
                   .get(Http::LowerCaseString("authorization"))
                   ->value()
                   .size());
  EXPECT_EQ(1UL,
            test_server_->gauge("http.config_test.aws_lambda.current_state")
                ->value());
  EXPECT_EQ(1UL,
            test_server_->counter("http.config_test.aws_lambda.creds_rotated")
                ->value());
  EXPECT_EQ(1UL,
            test_server_->counter("http.config_test.aws_lambda.creds_rotated")
                ->value());
  EXPECT_EQ(0UL,
            test_server_->counter("http.config_test.aws_lambda.fetch_failed")
                ->value());
}
} // namespace Envoy
