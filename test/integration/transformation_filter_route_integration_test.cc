#include "common/config/metadata.h"
#include "common/config/transformation_well_known_names.h"

#include "transformation_filter.pb.h"

#include "test/integration/http_integration.h"
#include "test/integration/integration.h"
#include "test/integration/utility.h"

namespace Envoy {

const std::string TRANSFORMATION_FILTER =
    R"EOF(
name: io.solo.transformation
config:
  route_specific_config: true
)EOF";

class TransformationFilterPerRouteIntegrationTest
    : public HttpIntegrationTest,
      public testing::TestWithParam<Network::Address::IpVersion> {
public:
  TransformationFilterPerRouteIntegrationTest()
      : HttpIntegrationTest(Http::CodecClient::Type::HTTP1, GetParam()) {}

  /**
   * Initializer for an individual integration test.
   */
  void initialize() override {
    config_helper_.addFilter(filter_string_);

    config_helper_.addConfigModifier(
        [this](envoy::config::filter::network::http_connection_manager::v2::
                   HttpConnectionManager &hcm) {

          auto& perFilterConfig = (*hcm.mutable_route_config()
                               ->mutable_virtual_hosts(0)
                               ->mutable_routes(0)
                               ->mutable_per_filter_config())[Config::TransformationFilterNames::get().TRANSFORMATION];

            envoy::api::v2::filter::http::RouteTransformations proto_config;

            auto& availableTransform = *proto_config.mutable_request_transformation();
            auto& transform = *availableTransform.mutable_transformation_template();
            transform.set_advanced_templates(true);
            auto& ext1 = (*transform.mutable_extractors())["ext1"];
            ext1.set_header(":path");
            ext1.set_header(":path");
            ext1.set_regex("/users/(\\d+)");
            ext1.set_subgroup(1);
            auto& header1 = (*transform.mutable_headers())["x-solo"];
            header1.set_text("solo.io");
            transform.mutable_body()->set_text("abc {{extraction(\"ext1\")}}");
            
            
            MessageUtil::jsonConvert(proto_config, perFilterConfig);

        });

    HttpIntegrationTest::initialize();

    codec_client_ =
        makeHttpConnection(makeClientConnection((lookupPort("http"))));
  }

  void processRequest(IntegrationStreamDecoderPtr& response, std::string body = "") {
    waitForNextUpstreamRequest();
    upstream_request_->encodeHeaders(
        Http::TestHeaderMapImpl{{":status", "200"}}, body.empty());

    if (!body.empty()) {
      Buffer::OwnedImpl data(body);
      upstream_request_->encodeData(data, true);
    }

    response->waitForEndStream();
  }

  std::string filter_string_{TRANSFORMATION_FILTER};
  bool transform_response_{false};
};

INSTANTIATE_TEST_CASE_P(
    IpVersions, TransformationFilterPerRouteIntegrationTest,
    testing::ValuesIn(TestEnvironment::getIpVersionsForTest()));

TEST_P(TransformationFilterPerRouteIntegrationTest, TransformHeaderOnlyRequest) {
  initialize();
  Http::TestHeaderMapImpl request_headers{{":method", "GET"},
                                          {":authority", "www.solo.io"},
                                          {":path", "/users/234"}};

  auto response = codec_client_->makeHeaderOnlyRequest(request_headers);
  processRequest(response);

  EXPECT_STREQ("solo.io", upstream_request_->headers()
                              .get(Http::LowerCaseString("x-solo"))
                              ->value()
                              .c_str());
  std::string body = TestUtility::bufferToString(upstream_request_->body());
  EXPECT_EQ("abc 234", body);
}


} // namespace Envoy
