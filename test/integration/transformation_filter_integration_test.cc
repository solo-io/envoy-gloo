#include "common/config/transformation_well_known_names.h"
#include "common/config/metadata.h"

#include "test/integration/http_integration.h"
#include "test/integration/integration.h"
#include "test/integration/utility.h"

namespace Envoy {

const std::string DEFAULT_TRANSFORMATION_FILTER =
    R"EOF(
name: io.solo.transformation
)EOF";

class TransformationFilterIntegrationTest
    : public Envoy::HttpIntegrationTest,
      public testing::TestWithParam<Envoy::Network::Address::IpVersion> {
public:
  TransformationFilterIntegrationTest()
      : Envoy::HttpIntegrationTest(Envoy::Http::CodecClient::Type::HTTP1,
                                   GetParam()) {}

  /**
   * Initializer for an individual integration test.
   */
  void initialize() override {
    config_helper_.addFilter(DEFAULT_TRANSFORMATION_FILTER);

    config_helper_.addConfigModifier(
        [](envoy::config::bootstrap::v2::Bootstrap &/*bootstrap*/) {
        });

    config_helper_.addConfigModifier(
        [](envoy::config::filter::network::http_connection_manager::v2::
               HttpConnectionManager &/*hcm*/) {
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
    IpVersions, TransformationFilterIntegrationTest,
    testing::ValuesIn(Envoy::TestEnvironment::getIpVersionsForTest()));

TEST_P(TransformationFilterIntegrationTest, Test1) {
  Envoy::Http::TestHeaderMapImpl request_headers{
      {":method", "POST"}, {":authority", "www.solo.io"}, {":path", "/"}};

  sendRequestAndWaitForResponse(request_headers, 10, default_response_headers_,
                                10);
/*
  EXPECT_NE(0, upstream_request_->headers()
                   .get(Envoy::Http::LowerCaseString("authorization"))
                   ->value()
                   .size());
*/
}
} // namespace Envoy
