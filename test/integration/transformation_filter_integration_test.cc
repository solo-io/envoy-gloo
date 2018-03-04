#include "common/config/metadata.h"
#include "common/config/transformation_well_known_names.h"

#include "test/integration/http_integration.h"
#include "test/integration/integration.h"
#include "test/integration/utility.h"

namespace Envoy {

const std::string DEFAULT_TRANSFORMATION_FILTER =
    R"EOF(
name: io.solo.transformation
config:
  advanced_templates: true
  transformations:
    translation1:
      extractors:
        ext1:
          header: :path
          regex: /users/(\d+)
          subgroup: 1
      request_template:
        headers:
          x-solo:
            text: solo.io
        body:
          text: abc {{extraction("ext1")}}
)EOF";

const std::string BODY_TRANSFORMATION_FILTER =
    R"EOF(
name: io.solo.transformation
config:
  advanced_templates: true
  transformations:
    translation1:
      request_template:
        body:
          text: "{{abc}}"
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
    config_helper_.addFilter(filter_string_);

    config_helper_.addConfigModifier(
        [](envoy::config::bootstrap::v2::Bootstrap & /*bootstrap*/) {});

    config_helper_.addConfigModifier(
        [this](envoy::config::filter::network::http_connection_manager::v2::
                   HttpConnectionManager &hcm) {

          auto *metadata = hcm.mutable_route_config()
                               ->mutable_virtual_hosts(0)
                               ->mutable_routes(0)
                               ->mutable_metadata();

          Config::Metadata::mutableMetadataValue(
              *metadata,
              Config::TransformationMetadataFilters::get().TRANSFORMATION,
              Config::MetadataTransformationKeys::get().REQUEST_TRANSFORMATION)
              .set_string_value("translation1");

          if (transform_response_) {
            Config::Metadata::mutableMetadataValue(
                *metadata,
                Config::TransformationMetadataFilters::get().TRANSFORMATION,
                Config::MetadataTransformationKeys::get()
                    .RESPONSE_TRANSFORMATION)
                .set_string_value("translation1");
          }
        });

    HttpIntegrationTest::initialize();

    codec_client_ =
        makeHttpConnection(makeClientConnection((lookupPort("http"))));
  }

  void processRequest(std::string body = "") {
    waitForNextUpstreamRequest();
    upstream_request_->encodeHeaders(
        Http::TestHeaderMapImpl{{":status", "200"}}, body.empty());

    if (!body.empty()) {
      Buffer::OwnedImpl data(body);
      upstream_request_->encodeData(data, true);
    }

    response_->waitForEndStream();
  }

  std::string filter_string_{DEFAULT_TRANSFORMATION_FILTER};
  bool transform_response_{false};
};

INSTANTIATE_TEST_CASE_P(
    IpVersions, TransformationFilterIntegrationTest,
    testing::ValuesIn(Envoy::TestEnvironment::getIpVersionsForTest()));

TEST_P(TransformationFilterIntegrationTest, TransformHeaderOnlyRequest) {
  initialize();
  Envoy::Http::TestHeaderMapImpl request_headers{{":method", "GET"},
                                                 {":authority", "www.solo.io"},
                                                 {":path", "/users/234"}};

  codec_client_->makeHeaderOnlyRequest(request_headers, *response_);
  processRequest();

  EXPECT_STREQ("solo.io", upstream_request_->headers()
                              .get(Envoy::Http::LowerCaseString("x-solo"))
                              ->value()
                              .c_str());
  std::string body = TestUtility::bufferToString(upstream_request_->body());
  EXPECT_EQ("abc 234", body);
}

TEST_P(TransformationFilterIntegrationTest, TransformHeadersAndBodyRequest) {
  filter_string_ = BODY_TRANSFORMATION_FILTER;
  initialize();
  Envoy::Http::TestHeaderMapImpl request_headers{
      {":method", "POST"}, {":authority", "www.solo.io"}, {":path", "/users"}};
  auto downstream_request =
      &codec_client_->startRequest(request_headers, *response_);
  Buffer::OwnedImpl data("{\"abc\":\"efg\"}");
  codec_client_->sendData(*downstream_request, data, true);

  processRequest();

  std::string body = TestUtility::bufferToString(upstream_request_->body());
  EXPECT_EQ("efg", body);
}

TEST_P(TransformationFilterIntegrationTest, TransformResponseBadRequest) {
  transform_response_ = true;
  filter_string_ = BODY_TRANSFORMATION_FILTER;
  initialize();
  Envoy::Http::TestHeaderMapImpl request_headers{
      {":method", "POST"}, {":authority", "www.solo.io"}, {":path", "/users"}};
  auto downstream_request =
      &codec_client_->startRequest(request_headers, *response_);
  Buffer::OwnedImpl data("{\"abc\":\"efg\"}");
  codec_client_->sendData(*downstream_request, data, true);

  processRequest();

  std::string body = TestUtility::bufferToString(upstream_request_->body());
  EXPECT_EQ("efg", body);
  std::string rbody = response_->body();
  EXPECT_NE(std::string::npos, rbody.find("bad request"));
}

TEST_P(TransformationFilterIntegrationTest, TransformResponse) {
  transform_response_ = true;
  filter_string_ = BODY_TRANSFORMATION_FILTER;
  initialize();
  Envoy::Http::TestHeaderMapImpl request_headers{
      {":method", "POST"}, {":authority", "www.solo.io"}, {":path", "/users"}};
  auto downstream_request =
      &codec_client_->startRequest(request_headers, *response_);
  Buffer::OwnedImpl data("{\"abc\":\"efg\"}");
  codec_client_->sendData(*downstream_request, data, true);

  processRequest("{\"abc\":\"soloio\"}");

  std::string rbody = response_->body();
  EXPECT_EQ("soloio", rbody);
}

} // namespace Envoy
