#include "source/common/config/metadata.h"

#include "source/extensions/filters/http/solo_well_known_names.h"

#include "test/integration/http_integration.h"
#include "test/integration/http_protocol_integration.h"
#include "test/integration/integration.h"
#include "test/integration/utility.h"
#include "nlohmann/json.hpp"

#include "api/envoy/config/filter/http/transformation/v2/transformation_filter.pb.validate.h"
#include "fmt/printf.h"

using ::envoy::extensions::filters::network::http_connection_manager::v3::
    HttpFilter;

namespace Envoy {

const std::string DEFAULT_TRANSFORMATION =
    R"EOF(
  request_transformation:
    ai_transformation: {}
)EOF";

const std::string DEFAULT_FILTER_TRANSFORMATION =
    R"EOF(
      {}
)EOF";

const std::string DEFAULT_MATCHER =
    R"EOF(
    prefix: /
)EOF";

#if 0
class AiTransformationIntegrationTest
    : public HttpIntegrationTest,
      public testing::TestWithParam<Network::Address::IpVersion> {
public:
  AiTransformationIntegrationTest()
      : HttpIntegrationTest(Http::CodecClient::Type::HTTP1, GetParam()) {}
#else
class AiTransformationIntegrationTest
  : public HttpProtocolIntegrationTest {
    public:
      AiTransformationIntegrationTest() : HttpProtocolIntegrationTest() {}
      /* HttpIntegrationTest(Http::CodecClient::Type::HTTP1, GetParam(), simTime()) {} */
#endif
  /**
   * Initializer for an individual integration test.
   */
  void initialize() override {

    const std::string default_filter =
        loadListenerConfig(filter_transformation_string_, matcher_string_);
      
    setUpstreamProtocol(Http::CodecType::HTTP1);
    config_helper_.prependFilter(default_filter, downstream_filter_);


    if (!downstream_filter_) {
      HttpFilter filter;
      filter.set_name(
          Extensions::HttpFilters::SoloHttpFilterNames::get().Wait);
      config_helper_.prependFilter(MessageUtil::getJsonStringFromMessageOrError(filter), downstream_filter_);
      addEndpointMeta();
    }

    if (transformation_string_ != "") {
      // set the default transformation
      config_helper_.addConfigModifier(
          [this](envoy::extensions::filters::network::http_connection_manager::
                     v3::HttpConnectionManager &hcm) {
            auto &mostSpecificPerFilterConfig = (*hcm.mutable_route_config()
                                          ->mutable_virtual_hosts(0)
                                          ->mutable_routes(0)
                                          ->mutable_typed_per_filter_config())
                [Extensions::HttpFilters::SoloHttpFilterNames::get()
                     .Transformation];
            envoy::api::v2::filter::http::RouteTransformations transformations;
            TestUtility::loadFromYaml(transformation_string_, transformations);
            mostSpecificPerFilterConfig.PackFrom(transformations);
          });
    }

    HttpIntegrationTest::initialize();

    codec_client_ =
        makeHttpConnection(makeClientConnection((lookupPort("http"))));
  }

  void processRequest(IntegrationStreamDecoderPtr &response,
                      std::string body = "") {
    auto start = std::chrono::steady_clock::now();
    waitForNextUpstreamRequest(0, std::chrono::milliseconds(100000));
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "waited (microseconds): " << duration.count() << std::endl;
    upstream_request_->encodeHeaders(
        Http::TestRequestHeaderMapImpl{{":status", "200"}}, body.empty());

    if (!body.empty()) {
      Buffer::OwnedImpl data(body);
      upstream_request_->encodeData(data, true);
    }

    ASSERT_TRUE(response->waitForEndStream());
  }

  std::string transformation_string_{DEFAULT_TRANSFORMATION};
  std::string filter_transformation_string_{DEFAULT_FILTER_TRANSFORMATION};
  std::string matcher_string_{DEFAULT_MATCHER};
  bool downstream_filter_{true};

private:
  std::string loadListenerConfig(const std::string &transformation_config_str,
                                 const std::string &matcher_str) {

    envoy::api::v2::filter::http::TransformationRule transformation_rule;
    envoy::api::v2::filter::http::TransformationRule_Transformations
        route_transformations;
    TestUtility::loadFromYaml(transformation_config_str, route_transformations);

    envoy::config::route::v3::RouteMatch route_match;
    TestUtility::loadFromYaml(matcher_str, route_match);

    *transformation_rule.mutable_route_transformations() =
        route_transformations;
    *transformation_rule.mutable_match() = route_match;

    envoy::api::v2::filter::http::FilterTransformations filter_config;
    *filter_config.mutable_transformations()->Add() = transformation_rule;

    HttpFilter filter;
    filter.set_name(
        Extensions::HttpFilters::SoloHttpFilterNames::get().Transformation);
    filter.mutable_typed_config()->PackFrom(filter_config);

    return MessageUtil::getJsonStringFromMessageOrError(filter);
  }

  void addEndpointMeta() {
    config_helper_.addConfigModifier(
      [](envoy::config::bootstrap::v3::Bootstrap& bootstrap) {
        
        auto* static_resources = bootstrap.mutable_static_resources();
        for (int i = 0; i < static_resources->clusters_size(); ++i) {
          auto* cluster = static_resources->mutable_clusters(i);
          for (int j = 0; j < cluster->load_assignment().endpoints_size(); ++j) {
            auto* endpoint = cluster->mutable_load_assignment()->mutable_endpoints(j);
            for (int k = 0; k < endpoint->lb_endpoints_size(); ++k) {
              auto* lb_endpoint = endpoint->mutable_lb_endpoints(k);
              auto* metadata = lb_endpoint->mutable_metadata();
              ProtobufWkt::Value value_obj;
              value_obj.set_string_value("bar");
              ProtobufWkt::Struct struct_obj;
              (*struct_obj.mutable_fields())["foo"] = value_obj;
              (*metadata->mutable_filter_metadata())[Extensions::HttpFilters::SoloHttpFilterNames::get().Transformation].MergeFrom(struct_obj);
            }
          }
        }
      });
  }
};

// INSTANTIATE_TEST_SUITE_P(
//     IpVersions, AiTransformationIntegrationTest,
//     testing::ValuesIn(TestEnvironment::getIpVersionsForTest()));
INSTANTIATE_TEST_SUITE_P(
  Protocols, AiTransformationIntegrationTest,
  testing::ValuesIn(HttpProtocolIntegrationTest::getProtocolTestParamsWithoutHTTP3()),
  HttpProtocolIntegrationTest::protocolTestParamsToString);

TEST_P(AiTransformationIntegrationTest, TransformHeaderOnlyRequestUpstream) {
  downstream_filter_ = false;
  initialize();
  Http::TestRequestHeaderMapImpl request_headers{{":method", "GET"},
                                                 {":authority", "www.solo.io"},
                                                 {":path", "/users/234"}};

  auto response = codec_client_->makeHeaderOnlyRequest(request_headers);
  processRequest(response);
  std::string path(
      upstream_request_->headers().Path()->value().getStringView());

  EXPECT_EQ("/v1/chat/completions", path);
  EXPECT_TRUE(response->complete());

}

} // namespace Envoy