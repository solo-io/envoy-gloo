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
  transformations:
    translation1:
      transformation_template:
        advanced_templates: true
        extractors:
          ext1:
            header: :path
            regex: /users/(\d+)
            subgroup: 1
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
  transformations:
    translation1:
      transformation_template:
        advanced_templates: true
        body:
          text: "{{abc}}"
)EOF";

const std::string PATH_TO_PATH_TRANSFORMATION_FILTER =
    R"EOF(
name: io.solo.transformation
config:
  transformations:
    translation1:
      transformation_template:
        advanced_templates: false
        extractors:
          ext1:
            header: :path
            regex: /users/(\d+)
            subgroup: 1
        headers: { ":path": {"text": "/solo/{{ext1}}"} }
        body:
          text: soloio
)EOF";

const std::string EMPTY_BODY_TRANSFORMATION_FILTER =
    R"EOF(
name: io.solo.transformation
config:
  transformations:
    translation1:
      transformation_template:
        advanced_templates: false
        body:
          text: ""
)EOF";

const std::string PASSTHROUGH_TRANSFORMATION_FILTER =
    R"EOF(
name: io.solo.transformation
config:
  transformations:
    translation1:
      transformation_template:
        advanced_templates: true
        extractors:
          ext1:
            header: :path
            regex: /users/(\d+)
            subgroup: 1
        headers: { "x-solo": {"text": "{{extraction(\"ext1\")}}"} }
        passthrough: {}
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

TEST_P(TransformationFilterIntegrationTest, TransformPathToOtherPath) {
  filter_string_ = PATH_TO_PATH_TRANSFORMATION_FILTER;
  initialize();
  Envoy::Http::TestHeaderMapImpl request_headers{{":method", "GET"},
                                                 {":authority", "www.solo.io"},
                                                 {":path", "/users/234"}};

  codec_client_->makeHeaderOnlyRequest(request_headers, *response_);
  processRequest();

  EXPECT_STREQ("/solo/234",
               upstream_request_->headers().Path()->value().c_str());
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
  // TODO add another test that the upstream body was not changed
  processRequest("{\"abc\":\"soloio\"}");

  std::string rbody = response_->body();
  EXPECT_EQ("soloio", rbody);
}

TEST_P(TransformationFilterIntegrationTest, RemoveBodyFromRequest) {
  filter_string_ = EMPTY_BODY_TRANSFORMATION_FILTER;
  transform_response_ = true;
  initialize();
  Envoy::Http::TestHeaderMapImpl request_headers{{":method", "POST"},
                                                 {":authority", "www.solo.io"},
                                                 {":path", "/empty-body-test"}};
  auto downstream_request =
      &codec_client_->startRequest(request_headers, *response_);
  Buffer::OwnedImpl data("{\"abc\":\"efg\"}");
  codec_client_->sendData(*downstream_request, data, true);

  processRequest("{\"abc\":\"soloio\"}");

  std::string rbody = response_->body();
  const auto &rheaders = response_->headers();
  EXPECT_EQ("", rbody);
  EXPECT_EQ(nullptr, rheaders.TransferEncoding());
  EXPECT_EQ(nullptr, rheaders.ContentType());
  if (rheaders.ContentLength() != nullptr) {
    EXPECT_STREQ("0", rheaders.ContentLength()->value().c_str());
  }
  // verify response

  std::string body = TestUtility::bufferToString(upstream_request_->body());
  const auto &headers = upstream_request_->headers();
  EXPECT_EQ("", body);
  EXPECT_EQ(nullptr, headers.TransferEncoding());
  EXPECT_EQ(nullptr, headers.ContentType());
  if (headers.ContentLength() != nullptr) {
    EXPECT_STREQ("0", headers.ContentLength()->value().c_str());
  }
}

TEST_P(TransformationFilterIntegrationTest, PassthroughBody) {
  filter_string_ = PASSTHROUGH_TRANSFORMATION_FILTER;
  initialize();
  std::string origBody = "{\"abc\":\"efg\"}";
  Envoy::Http::TestHeaderMapImpl request_headers{{":method", "GET"},
                                                 {":authority", "www.solo.io"},
                                                 {":path", "/users/12347"}};
  auto downstream_request =
      &codec_client_->startRequest(request_headers, *response_);
  Buffer::OwnedImpl data(origBody);
  codec_client_->sendData(*downstream_request, data, true);

  processRequest();

  EXPECT_STREQ("12347", upstream_request_->headers()
                            .get(Envoy::Http::LowerCaseString("x-solo"))
                            ->value()
                            .c_str());
  std::string body = TestUtility::bufferToString(upstream_request_->body());
  EXPECT_EQ(origBody, body);
}

} // namespace Envoy
