#include "common/config/metadata.h"
#include "common/config/solo_well_known_names.h"

#include "test/integration/http_integration.h"
#include "test/integration/integration.h"
#include "test/integration/utility.h"

#include "metadata_function_retriever.h"

namespace Envoy {

const std::string DEFAULT_LAMBDA_FILTER =
    R"EOF(
name: io.solo.lambda
config:
    access_key: a
    secret_key: b
)EOF";

class LambdaFilterIntegrationTest
    : public Envoy::HttpIntegrationTest,
      public testing::TestWithParam<Envoy::Network::Address::IpVersion> {
public:
  LambdaFilterIntegrationTest()
      : Envoy::HttpIntegrationTest(Envoy::Http::CodecClient::Type::HTTP1,
                                   GetParam()) {}

  /**
   * Initializer for an individual integration test.
   */
  void initialize() override {

    config_helper_.addFilter(DEFAULT_LAMBDA_FILTER);

    config_helper_.addConfigModifier([](envoy::api::v2::Bootstrap &bootstrap) {
      auto &lambda_cluster =
          (*bootstrap.mutable_static_resources()->mutable_clusters())[0];

      auto *metadata = lambda_cluster.mutable_metadata();

      Config::Metadata::mutableMetadataValue(
          *metadata, Config::SoloMetadataFilters::get().LAMBDA,
          Config::MetadataLambdaKeys::get().FUNC_NAME)
          .set_string_value("FunctionName");

      Config::Metadata::mutableMetadataValue(
          *metadata, Config::SoloMetadataFilters::get().LAMBDA,
          Config::MetadataLambdaKeys::get().HOSTNAME)
          .set_string_value("lambda.us-east-1.amazonaws.com");

      Config::Metadata::mutableMetadataValue(
          *metadata, Config::SoloMetadataFilters::get().LAMBDA,
          Config::MetadataLambdaKeys::get().REGION)
          .set_string_value("us-east-1");
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
    testing::ValuesIn(Envoy::TestEnvironment::getIpVersionsForTest()));

TEST_P(LambdaFilterIntegrationTest, Test1) {
  Envoy::Http::TestHeaderMapImpl headers{
      {":method", "POST"}, {":authority", "www.solo.io"}, {":path", "/"}};

  Envoy::IntegrationStreamDecoderPtr response(
      new Envoy::IntegrationStreamDecoder(*dispatcher_));
  Envoy::FakeStreamPtr request_stream;

  Envoy::Http::StreamEncoder &stream =
      codec_client_->startRequest(headers, *response);
  Envoy::Buffer::OwnedImpl data;
  data.add(std::string("{\"a\":123}"));
  codec_client_->sendData(stream, data, true);

  Envoy::FakeHttpConnectionPtr fake_upstream_connection =
      fake_upstreams_[0]->waitForHttpConnection(*dispatcher_);
  request_stream = fake_upstream_connection->waitForNewStream(*dispatcher_);
  request_stream->waitForEndStream(*dispatcher_);
  response->waitForEndStream();

  EXPECT_NE(0, request_stream->headers()
                   .get(Envoy::Http::LowerCaseString("authorization"))
                   ->value()
                   .size());

  codec_client_->close();
}
} // namespace Envoy
