#include "test/integration/integration.h"
#include "test/integration/utility.h"

namespace Solo {
class LambdaFilterIntegrationTest : public BaseIntegrationTest,
                                        public testing::TestWithParam<Network::Address::IpVersion> {
public:
  LambdaFilterIntegrationTest() : BaseIntegrationTest(GetParam()) {}
  /**
   * Initializer for an individual integration test.
   */
  void SetUp() override {
    fake_upstreams_.emplace_back(new FakeUpstream(0, FakeHttpConnection::Type::HTTP1, version_));
    registerPort("upstream_0", fake_upstreams_.back()->localAddress()->ip()->port());
    createTestServer("config-example.json", {"http"});
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
                        testing::ValuesIn(TestEnvironment::getIpVersionsForTest()));

TEST_P(LambdaFilterIntegrationTest, Test1) {
  Http::TestHeaderMapImpl headers{{":method", "POST"}, {":path", "/"}w;

  IntegrationCodecClientPtr codec_client;
  FakeHttpConnectionPtr fake_upstream_connection;
  IntegrationStreamDecoderPtr response(new IntegrationStreamDecoder(*dispatcher_));
  FakeStreamPtr request_stream;

  codec_client = makeHttpConnection(lookupPort("http"), Http::CodecClient::Type::HTTP1);
  codec_client->makeRequestWithBody(headers,2, *response);
  fake_upstream_connection = fake_upstreams_[0]->waitForHttpConnection(*dispatcher_);
  request_stream = fake_upstream_connection->waitForNewStream();
  request_stream->waitForEndStream(*dispatcher_);
  response->waitForEndStream();

  EXPECT_NE(0,
               request_stream->headers().get(Http::LowerCaseString("authorization"))->value().size());

  codec_client->close();
}
} // Solo
