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

class AWSLambdaFilterIntegrationTest
    : public HttpIntegrationTest,
      public testing::TestWithParam<Network::Address::IpVersion> {
public:
  AWSLambdaFilterIntegrationTest()
      : HttpIntegrationTest(Http::CodecClient::Type::HTTP1, GetParam(),
                            realTime()) {}

  /**
   * Initializer for an individual integration test.
   */
  void initialize() override {
    config_helper_.addFilter(DEFAULT_LAMBDA_FILTER);

    config_helper_.addConfigModifier([](envoy::config::bootstrap::v2::Bootstrap
                                            &bootstrap) {
      envoy::config::filter::http::aws_lambda::v2::AWSLambdaProtocolExtension
          protoextconfig;
      protoextconfig.set_host("lambda.us-east-1.amazonaws.com");
      protoextconfig.set_region("us-east-1");
      protoextconfig.set_access_key("access key");
      protoextconfig.set_secret_key("secret key");
      ProtobufWkt::Struct functionstruct;

      auto &lambda_cluster =
          (*bootstrap.mutable_static_resources()->mutable_clusters(0));

      auto &cluster_struct =
          (*lambda_cluster.mutable_extension_protocol_options())
              [Extensions::HttpFilters::SoloHttpFilterNames::get().AwsLambda];
      MessageUtil::jsonConvert(protoextconfig, cluster_struct);
    });

    config_helper_.addConfigModifier(
        [](envoy::config::filter::network::http_connection_manager::v2::
               HttpConnectionManager &hcm) {
          auto &perFilterConfig = (*hcm.mutable_route_config()
                                        ->mutable_virtual_hosts(0)
                                        ->mutable_routes(0)
                                        ->mutable_per_filter_config())
              [Extensions::HttpFilters::SoloHttpFilterNames::get().AwsLambda];

          envoy::config::filter::http::aws_lambda::v2::AWSLambdaPerRoute
              proto_config;
          proto_config.set_name("FunctionName");
          proto_config.set_qualifier("v1");

          MessageUtil::jsonConvert(proto_config, perFilterConfig);
        });

    HttpIntegrationTest::initialize();

    codec_client_ =
        makeHttpConnection(makeClientConnection((lookupPort("http"))));
  }

  /**
   * Initialize before every test.
   */
  void SetUp() override { initialize(); }
};

INSTANTIATE_TEST_SUITE_P(
    IpVersions, AWSLambdaFilterIntegrationTest,
    testing::ValuesIn(TestEnvironment::getIpVersionsForTest()));

TEST_P(AWSLambdaFilterIntegrationTest, Test1) {
  Http::TestHeaderMapImpl request_headers{
      {":method", "POST"}, {":authority", "www.solo.io"}, {":path", "/"}};

  sendRequestAndWaitForResponse(request_headers, 10, default_response_headers_,
                                10);

  EXPECT_NE(0, upstream_request_->headers()
                   .get(Http::LowerCaseString("authorization"))
                   ->value()
                   .size());
}
} // namespace Envoy
