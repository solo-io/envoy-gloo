#include "test/integration/integration.h"
#include "test/integration/utility.h"

#include "test/integration/http_integration.h"

namespace Solo {
class LambdaFilterIntegrationTest : public Envoy::HttpIntegrationTest,
                                        public testing::TestWithParam<Envoy::Network::Address::IpVersion> {
public:
  LambdaFilterIntegrationTest() : Envoy::HttpIntegrationTest(Envoy::Http::CodecClient::Type::HTTP1, GetParam()) {}
  /**
   * Initializer for an individual integration test.
   */
  void SetUp() override {
    fake_upstreams_.emplace_back(new Envoy::FakeUpstream(0, Envoy::FakeHttpConnection::Type::HTTP1, version_));
    registerPort("upstream_0", fake_upstreams_.back()->localAddress()->ip()->port());
    createTestServer("envoy-test.conf", {"http"});
  }

  /**
   * Destructor for an individual integration test.
   */
  void TearDown() override {
    test_server_.reset();
    fake_upstreams_.clear();
  }
};

INSTANTIATE_TEST_CASE_P(IpVersions, LambdaFilterIntegrationTest,
                        testing::ValuesIn(Envoy::TestEnvironment::getIpVersionsForTest()));

TEST_P(LambdaFilterIntegrationTest, Test1) {
  Envoy::Http::TestHeaderMapImpl headers{{":method", "POST"}, {":authority", "www.solo.io"}, {":path", "/"}};

  Envoy::IntegrationCodecClientPtr codec_client;
  Envoy::FakeHttpConnectionPtr fake_upstream_connection;
  Envoy::IntegrationStreamDecoderPtr response(new Envoy::IntegrationStreamDecoder(*dispatcher_));
  Envoy::FakeStreamPtr request_stream;

  codec_client = makeHttpConnection(lookupPort("http"));
  Envoy::Http::StreamEncoder& stream = codec_client->startRequest(headers, *response);
  Envoy::Buffer::OwnedImpl data;
  data.add(std::string("{\"a\":123}"));
  codec_client->sendData(stream, data, true);


  fake_upstream_connection = fake_upstreams_[0]->waitForHttpConnection(*dispatcher_);
  request_stream = fake_upstream_connection->waitForNewStream(*dispatcher_);
  request_stream->waitForEndStream(*dispatcher_);
  response->waitForEndStream();

  EXPECT_NE(0, request_stream->headers().get(Envoy::Http::LowerCaseString("authorization"))->value().size());

  codec_client->close();
}
} // Solo
