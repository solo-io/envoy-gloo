#include "common/config/lambda_well_known_names.h"
#include "common/config/metadata.h"

#include "test/integration/http_integration.h"
#include "test/integration/integration.h"
#include "test/integration/utility.h"

namespace Envoy {

const std::string DEFAULT_LAMBDA_FILTER =
    R"EOF(
name: io.solo.lambda
)EOF";

class LambdaFilterIntegrationTest
    : public HttpIntegrationTest,
      public testing::TestWithParam<Network::Address::IpVersion> {
public:
  LambdaFilterIntegrationTest()
      : HttpIntegrationTest(Http::CodecClient::Type::HTTP1, GetParam()) {}

  /**
   * Initializer for an individual integration test.
   */
  void initialize() override {
    config_helper_.addFilter(DEFAULT_LAMBDA_FILTER);

    config_helper_.addConfigModifier(
        [](envoy::config::bootstrap::v2::Bootstrap &bootstrap) {
          auto &lambda_cluster =
              (*bootstrap.mutable_static_resources()->mutable_clusters(0));

          auto *metadata = lambda_cluster.mutable_metadata();

          Config::Metadata::mutableMetadataValue(
              *metadata, Config::LambdaMetadataFilters::get().LAMBDA,
              Config::LambdaMetadataKeys::get().HOSTNAME)
              .set_string_value("lambda.us-east-1.amazonaws.com");

          Config::Metadata::mutableMetadataValue(
              *metadata, Config::LambdaMetadataFilters::get().LAMBDA,
              Config::LambdaMetadataKeys::get().REGION)
              .set_string_value("us-east-1");

          Config::Metadata::mutableMetadataValue(
              *metadata, Config::LambdaMetadataFilters::get().LAMBDA,
              Config::LambdaMetadataKeys::get().ACCESS_KEY)
              .set_string_value("access key");

          Config::Metadata::mutableMetadataValue(
              *metadata, Config::LambdaMetadataFilters::get().LAMBDA,
              Config::LambdaMetadataKeys::get().SECRET_KEY)
              .set_string_value("secret dont tell");

          /////
          // TODO(yuval-k): use consts (filename mess)
          std::string functionalfilter = "io.solo.function_router";
          std::string functionsKey = "functions";

          // add the function spec in the cluster config.
          // TODO(yuval-k): extract this to help method (test utils?)
          ProtobufWkt::Struct *functionstruct =
              Config::Metadata::mutableMetadataValue(
                  *metadata, functionalfilter, functionsKey)
                  .mutable_struct_value();

          ProtobufWkt::Value &functionstructspecvalue =
              (*functionstruct->mutable_fields())["funcname"];
          ProtobufWkt::Struct *functionsspecstruct =
              functionstructspecvalue.mutable_struct_value();

          (*functionsspecstruct
                ->mutable_fields())[Config::LambdaMetadataKeys::get().FUNC_NAME]
              .set_string_value("FunctionName");
          (*functionsspecstruct->mutable_fields())
              [Config::LambdaMetadataKeys::get().FUNC_QUALIFIER]
                  .set_string_value("v1");
        });

    config_helper_.addConfigModifier(
        [](envoy::config::filter::network::http_connection_manager::v2::
               HttpConnectionManager &hcm) {
          auto *metadata = hcm.mutable_route_config()
                               ->mutable_virtual_hosts(0)
                               ->mutable_routes(0)
                               ->mutable_metadata();
          std::string functionalfilter = "io.solo.function_router";
          std::string functionKey = "function";
          std::string clustername =
              hcm.route_config().virtual_hosts(0).routes(0).route().cluster();

          ProtobufWkt::Struct *clusterstruct =
              Config::Metadata::mutableMetadataValue(
                  *metadata, functionalfilter, clustername)
                  .mutable_struct_value();

          (*clusterstruct->mutable_fields())[functionKey].set_string_value(
              "funcname");

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

INSTANTIATE_TEST_CASE_P(
    IpVersions, LambdaFilterIntegrationTest,
    testing::ValuesIn(TestEnvironment::getIpVersionsForTest()));

TEST_P(LambdaFilterIntegrationTest, Test1) {
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
