#include "source/common/config/metadata.h"

#include "source/extensions/filters/http/solo_well_known_names.h"

#include "test/integration/http_integration.h"
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

const std::string DUPLICATE_HEADER_TRANSFORMATION =
    R"EOF(
  request_transformation:
    transformation_template:
      advanced_templates: true
      headers_to_append:
        - key: x-solo
          value:
            text: appended header 1
        - key: x-solo
          value:
            text: appended header 2
        - key: x-new-header
          value:
            text: new header
)EOF";

const std::string BODY_TRANSFORMATION =
    R"EOF(
  request_transformation:
    transformation_template:
      advanced_templates: true
      body:
        text: "{{abc}}"
)EOF";

const std::string BODY_RESPONSE_TRANSFORMATION =
    R"EOF(
  response_transformation:
    transformation_template:
      advanced_templates: true
      body:
        text: "{{abc}}"
)EOF";

const std::string BODY_REQUEST_RESPONSE_TRANSFORMATION =
    R"EOF(
  request_transformation:
    transformation_template:
      advanced_templates: true
      body:
        text: "{{abc}}"
  response_transformation:
    transformation_template:
      advanced_templates: true
      body:
        text: "{{abc}}"
)EOF";

const std::string PATH_TO_PATH_TRANSFORMATION =
    R"EOF(
  request_transformation:
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

const std::string EMPTY_BODY_REQUEST_RESPONSE_TRANSFORMATION =
    R"EOF(
  request_transformation:
    transformation_template:
      advanced_templates: false
      body:
        text: ""
  response_transformation:
    transformation_template:
      advanced_templates: false
      body:
        text: ""
)EOF";

const std::string PASSTHROUGH_TRANSFORMATION =
    R"EOF(
  request_transformation:
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

const std::string DEFAULT_FILTER_TRANSFORMATION =
    R"EOF(
      {}
)EOF";

const std::string DEFAULT_MATCHER =
    R"EOF(
    prefix: /
)EOF";

const std::string INVALID_MATCHER =
    R"EOF(
    prefix: /invalid
)EOF";

class TransformationFilterIntegrationTest
    : public HttpIntegrationTest,
      public testing::TestWithParam<Network::Address::IpVersion> {
public:
  TransformationFilterIntegrationTest()
      : HttpIntegrationTest(Http::CodecClient::Type::HTTP1, GetParam()) {}

  /**
   * Initializer for an individual integration test.
   */
  void initialize() override {

    const std::string default_filter =
        loadListenerConfig(filter_transformation_string_, matcher_string_);
    // set the default transformation
    config_helper_.addFilter(default_filter);

    if (transformation_string_ != "") {
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
    waitForNextUpstreamRequest();
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
};

INSTANTIATE_TEST_SUITE_P(
    IpVersions, TransformationFilterIntegrationTest,
    testing::ValuesIn(TestEnvironment::getIpVersionsForTest()));

TEST_P(TransformationFilterIntegrationTest, TransformHeaderOnlyRequest) {
  initialize();
  Http::TestRequestHeaderMapImpl request_headers{{":method", "GET"},
                                                 {":authority", "www.solo.io"},
                                                 {":path", "/users/234"},
                                                 {"x-solo", "original header (not preserved)"},
                                                 {"x-solo", "original header 2 (not preserved)"}};

  auto response = codec_client_->makeHeaderOnlyRequest(request_headers);
  processRequest(response);

  std::string xsolo_header(upstream_request_->headers()
                               .get(Http::LowerCaseString("x-solo"))[0]
                               ->value()
                               .getStringView());
  EXPECT_EQ("solo.io", xsolo_header);
  EXPECT_EQ(1, upstream_request_->headers().get(Http::LowerCaseString("x-solo")).size());
  std::string body = upstream_request_->body().toString();
  EXPECT_EQ("abc 234", body);
}

TEST_P(TransformationFilterIntegrationTest, KeepDuplicateHeaderRequest) {
  transformation_string_ = DUPLICATE_HEADER_TRANSFORMATION;
  initialize();
  Http::TestRequestHeaderMapImpl request_headers{{":method", "GET"},
                                                 {":authority", "www.solo.io"},
                                                 {":path", "/users/234"},
                                                 {"x-solo", "original header"}};

  auto response = codec_client_->makeHeaderOnlyRequest(request_headers);
  processRequest(response);

  const auto getHeader = [this](std::string header, int index) {
    return upstream_request_->headers()
                               .get(Http::LowerCaseString(header))[index]
                               ->value()
                               .getStringView();
  };


  EXPECT_EQ("original header", getHeader("x-solo", 0));
  EXPECT_EQ("appended header 1", getHeader("x-solo", 1));
  EXPECT_EQ("appended header 2", getHeader("x-solo", 2));
  EXPECT_EQ("new header", getHeader("x-new-header", 0));
}

TEST_P(TransformationFilterIntegrationTest, TransformPathToOtherPath) {
  transformation_string_ = PATH_TO_PATH_TRANSFORMATION;
  initialize();
  Http::TestRequestHeaderMapImpl request_headers{{":method", "GET"},
                                                 {":authority", "www.solo.io"},
                                                 {":path", "/users/234"}};

  auto response = codec_client_->makeHeaderOnlyRequest(request_headers);
  processRequest(response);

  std::string path(
      upstream_request_->headers().Path()->value().getStringView());

  EXPECT_EQ("/solo/234", path);
}

TEST_P(TransformationFilterIntegrationTest, TransformHeadersAndBodyRequest) {
  transformation_string_ = BODY_TRANSFORMATION;
  initialize();
  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "POST"}, {":authority", "www.solo.io"}, {":path", "/users"}};
  auto encoder_decoder = codec_client_->startRequest(request_headers);

  auto downstream_request = &encoder_decoder.first;
  auto response = std::move(encoder_decoder.second);

  Buffer::OwnedImpl data("{\"abc\":\"efg\"}");
  codec_client_->sendData(*downstream_request, data, true);

  processRequest(response);

  std::string body = upstream_request_->body().toString();
  EXPECT_EQ("efg", body);
}
TEST_P(TransformationFilterIntegrationTest, TransformResponseBadRequest) {
  transformation_string_ = BODY_REQUEST_RESPONSE_TRANSFORMATION;
  initialize();
  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "POST"}, {":authority", "www.solo.io"}, {":path", "/users"}};
  auto encoder_decoder = codec_client_->startRequest(request_headers);

  auto downstream_request = &encoder_decoder.first;
  auto response = std::move(encoder_decoder.second);
  Buffer::OwnedImpl data("{\"abc\":\"efg\"}");
  codec_client_->sendData(*downstream_request, data, true);

  processRequest(response);

  std::string body = upstream_request_->body().toString();
  EXPECT_EQ("efg", body);
  std::string rbody = response->body();
  EXPECT_NE(std::string::npos, rbody.find("bad request"));
}

TEST_P(TransformationFilterIntegrationTest, TransformResponse) {
  transformation_string_ = BODY_RESPONSE_TRANSFORMATION;
  initialize();
  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "POST"}, {":authority", "www.solo.io"}, {":path", "/users"}};
  auto encoder_decoder = codec_client_->startRequest(request_headers);

  auto downstream_request = &encoder_decoder.first;
  auto response = std::move(encoder_decoder.second);
  Buffer::OwnedImpl data("{\"abc\":\"efg\"}");
  codec_client_->sendData(*downstream_request, data, true);
  // TODO add another test that the upstream body was not changed
  processRequest(response, "{\"abc\":\"soloio\"}");

  std::string rbody = response->body();
  EXPECT_EQ("soloio", rbody);
}

TEST_P(TransformationFilterIntegrationTest, SkipTransformation) {
  transformation_string_ = "";
  filter_transformation_string_ = BODY_RESPONSE_TRANSFORMATION;
  matcher_string_ = INVALID_MATCHER;
  initialize();
  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "POST"}, {":authority", "www.solo.io"}, {":path", "/users"}};
  auto encoder_decoder = codec_client_->startRequest(request_headers);

  auto downstream_request = &encoder_decoder.first;
  auto response = std::move(encoder_decoder.second);
  Buffer::OwnedImpl data("{\"abc\":\"efg\"}");
  codec_client_->sendData(*downstream_request, data, true);
  // TODO add another test that the upstream body was not changed
  processRequest(response, "{\"abc\":\"soloio\"}");

  std::string rbody = response->body();
  EXPECT_EQ("{\"abc\":\"soloio\"}", rbody);
}

TEST_P(TransformationFilterIntegrationTest, RemoveBodyFromRequest) {
  transformation_string_ = EMPTY_BODY_REQUEST_RESPONSE_TRANSFORMATION;
  initialize();
  Http::TestRequestHeaderMapImpl request_headers{{":method", "POST"},
                                                 {":authority", "www.solo.io"},
                                                 {":path", "/empty-body-test"}};
  auto encoder_decoder = codec_client_->startRequest(request_headers);

  auto downstream_request = &encoder_decoder.first;
  auto response = std::move(encoder_decoder.second);

  Buffer::OwnedImpl data("{\"abc\":\"efg\"}");
  codec_client_->sendData(*downstream_request, data, true);

  processRequest(response, "{\"abc\":\"soloio\"}");

  std::string rbody = response->body();
  const auto &rheaders = response->headers();
  EXPECT_EQ("", rbody);
  EXPECT_EQ(nullptr, rheaders.TransferEncoding());
  EXPECT_EQ(nullptr, rheaders.ContentType());
  if (rheaders.ContentLength() != nullptr) {
    EXPECT_EQ("0", rheaders.ContentLength()->value().getStringView());
  }
  // verify response

  std::string body = upstream_request_->body().toString();
  const auto &headers = upstream_request_->headers();
  EXPECT_EQ("", body);
  EXPECT_EQ(nullptr, headers.TransferEncoding());
  EXPECT_EQ(nullptr, headers.ContentType());
  if (headers.ContentLength() != nullptr) {
    EXPECT_EQ("0", headers.ContentLength()->value().getStringView());
  }
}

TEST_P(TransformationFilterIntegrationTest, PassthroughBody) {
  transformation_string_ = PASSTHROUGH_TRANSFORMATION;
  initialize();
  std::string origBody = "{\"abc\":\"efg\"}";
  Http::TestRequestHeaderMapImpl request_headers{{":method", "GET"},
                                                 {":authority", "www.solo.io"},
                                                 {":path", "/users/12347"}};
  auto encoder_decoder = codec_client_->startRequest(request_headers);

  auto downstream_request = &encoder_decoder.first;
  auto response = std::move(encoder_decoder.second);

  Buffer::OwnedImpl data(origBody);
  codec_client_->sendData(*downstream_request, data, true);

  processRequest(response);

  EXPECT_EQ("12347", upstream_request_->headers()
                         .get(Http::LowerCaseString("x-solo"))[0]
                         ->value()
                         .getStringView());
  std::string body = upstream_request_->body().toString();
  EXPECT_EQ(origBody, body);
}

TEST_P(TransformationFilterIntegrationTest, BodyHeaderTransform) {
  using json = nlohmann::json;
  transformation_string_ =
    R"EOF(
  transformations:
  - request_match:
      request_transformation:
        header_body_transform:
          add_request_metadata: true
      response_transformation:
        header_body_transform:
          add_request_metadata: true
)EOF";
  initialize();
  std::string origBody = "{\"abc\":\"efg\"}";
  Http::TestRequestHeaderMapImpl request_headers{{":method", "GET"},
                                                 {":authority", "www.solo.io"},
                                                 {":path", "/users/12347"}};
  auto encoder_decoder = codec_client_->startRequest(request_headers);

  auto downstream_request = &encoder_decoder.first;
  auto response = std::move(encoder_decoder.second);

  Buffer::OwnedImpl data(origBody);
  codec_client_->sendData(*downstream_request, data, true);

  processRequest(response);

  json actual_request = json::parse(upstream_request_->body().toString());
  actual_request["headers"]["x-request-id"] = ""; // zero out random headers
  auto expected_request = R"(
  {
   "body":"{\"abc\":\"efg\"}",
   "headers": {
     ":authority":"www.solo.io",
     ":method":"GET",":path":"/users/12347",":scheme":"http",
     "x-forwarded-proto":"http","x-request-id":""
   },
   "httpMethod":"GET",
   "path":"/users/12347",
   "queryString":"",
    "multiValueHeaders": {},
    "multiValueQueryStringParameters": {},
    "queryStringParameters": {}
   }
)"_json;

  // make sure response works as well, with no metadata set:
  EXPECT_EQ(expected_request, actual_request);

  json actual_response = json::parse(response->body());
  // remove the `x-envoy-upstream-service-time` since its
  // value depends on how long the test took to run
  actual_response["headers"].erase("x-envoy-upstream-service-time");
  auto expected_response = R"(
  {
    "headers": {
      ":status":"200",
      "content-length":"0"
   }
   }
)"_json;
  EXPECT_EQ(expected_response, actual_response);

}

TEST_P(TransformationFilterIntegrationTest, RenderBodyAsJsonTransform) {
  using json = nlohmann::json;
  transformation_string_ =
    R"EOF(
  request_transformation:
    transformation_template:
      advanced_templates: false
      render_body_as_json: true
      body:
        text: "{\"Foo\":\"{{ foo }}\"}"
  response_transformation:
    transformation_template:
      advanced_templates: false
      render_body_as_json: true
      body:
        text: "{\"Bar\":\"{{ bar }}\"}"
)EOF";
  initialize();
  std::string origReqBody = R"EOF({"foo":"\"bar\""})EOF";
  Http::TestRequestHeaderMapImpl request_headers{{":method", "GET"},
                                                 {":authority", "www.solo.io"},
                                                 {":path", "/"}};
  auto encoder_decoder = codec_client_->startRequest(request_headers);

  auto downstream_request = &encoder_decoder.first;
  auto response = std::move(encoder_decoder.second);

  Buffer::OwnedImpl data(origReqBody);
  codec_client_->sendData(*downstream_request, data, true);

  processRequest(response, R"EOF({"bar":"\"baz\""})EOF");

  auto actual_request_body = json::parse(upstream_request_->body().toString());
  auto expected_request_body = R"({"Foo":"\"bar\""})"_json;
  EXPECT_EQ(expected_request_body, actual_request_body);

  // make sure response works as well
  auto actual_response_body = json::parse(response->body());
  auto expected_response_body = R"({"Bar":"\"baz\""})"_json;
  EXPECT_EQ(expected_response_body, actual_response_body);
}

TEST_P(TransformationFilterIntegrationTest, RenderBodyAsJsonTransformEscapedInput) {
  using json = nlohmann::json;
  transformation_string_ =
    R"EOF(
  request_transformation:
    transformation_template:
      advanced_templates: false
      render_body_as_json: false
      body:
        text: "{\"Foo\":\"{{ foo }}\"}"
  response_transformation:
    transformation_template:
      advanced_templates: false
      render_body_as_json: false
      body:
        text: "{\"Bar\":\"{{ bar }}\"}"
)EOF";
  initialize();
  std::string origReqBody = R"EOF({"foo":"\\\"bar\\\""})EOF";
  Http::TestRequestHeaderMapImpl request_headers{{":method", "GET"},
                                                 {":authority", "www.solo.io"},
                                                 {":path", "/"}};
  auto encoder_decoder = codec_client_->startRequest(request_headers);

  auto downstream_request = &encoder_decoder.first;
  auto response = std::move(encoder_decoder.second);

  Buffer::OwnedImpl data(origReqBody);
  codec_client_->sendData(*downstream_request, data, true);

  processRequest(response, R"EOF({"bar":"\\\"baz\\\""})EOF");

  auto actual_request_body = json::parse(upstream_request_->body().toString());
  auto expected_request_body = R"({"Foo":"\"bar\""})"_json;
  EXPECT_EQ(expected_request_body, actual_request_body);

  // make sure response works as well
  auto actual_response_body = json::parse(response->body());
  auto expected_response_body = R"({"Bar":"\"baz\""})"_json;
  EXPECT_EQ(expected_response_body, actual_response_body);
}

TEST_P(TransformationFilterIntegrationTest, RenderBodyAsJsonRawStringCallback) {
  using json = nlohmann::json;
  transformation_string_ =
    R"EOF(
  request_transformation:
    transformation_template:
      advanced_templates: false
      render_body_as_json: false
      body:
        text: "{\"Foo\":\"{{ raw_string(foo) }}\"}"
  response_transformation:
    transformation_template:
      advanced_templates: false
      render_body_as_json: false
      body:
        text: "{\"Bar\":\"{{ raw_string(bar) }}\"}"
)EOF";
  initialize();
  std::string origReqBody = R"EOF({"foo":"\"bar\""})EOF";
  Http::TestRequestHeaderMapImpl request_headers{{":method", "GET"},
                                                 {":authority", "www.solo.io"},
                                                 {":path", "/"}};
  auto encoder_decoder = codec_client_->startRequest(request_headers);

  auto downstream_request = &encoder_decoder.first;
  auto response = std::move(encoder_decoder.second);

  Buffer::OwnedImpl data(origReqBody);
  codec_client_->sendData(*downstream_request, data, true);

  processRequest(response, R"EOF({"bar":"\"baz\""})EOF");

  auto actual_request_body = json::parse(upstream_request_->body().toString());
  auto expected_request_body = R"({"Foo":"\"bar\""})"_json;
  EXPECT_EQ(expected_request_body, actual_request_body);

  // make sure response works as well
  auto actual_response_body = json::parse(response->body());
  auto expected_response_body = R"({"Bar":"\"baz\""})"_json;
  EXPECT_EQ(expected_response_body, actual_response_body);
}

TEST_P(TransformationFilterIntegrationTest, RenderBodyAsJsonTransformRawStringCallback) {
  using json = nlohmann::json;
  transformation_string_ =
    R"EOF(
  request_transformation:
    transformation_template:
      advanced_templates: false
      render_body_as_json: true
      body:
        text: "{\"Foo\":\"{{ raw_string(foo) }}\"}"
  response_transformation:
    transformation_template:
      advanced_templates: false
      render_body_as_json: true
      body:
        text: "{\"Bar\":\"{{ raw_string(bar) }}\"}"
)EOF";
  initialize();
  std::string origReqBody = R"EOF({"foo":"\"bar\""})EOF";
  Http::TestRequestHeaderMapImpl request_headers{{":method", "GET"},
                                                 {":authority", "www.solo.io"},
                                                 {":path", "/"}};
  auto encoder_decoder = codec_client_->startRequest(request_headers);

  auto downstream_request = &encoder_decoder.first;
  auto response = std::move(encoder_decoder.second);

  Buffer::OwnedImpl data(origReqBody);
  codec_client_->sendData(*downstream_request, data, true);

  processRequest(response, R"EOF({"bar":"\"baz\""})EOF");

  auto actual_request_body = json::parse(upstream_request_->body().toString());
  auto expected_request_body = R"({"Foo":"\\\"bar\\\""})"_json;
  EXPECT_EQ(expected_request_body, actual_request_body);

  // make sure response works as well
  auto actual_response_body = json::parse(response->body());
  auto expected_response_body = R"({"Bar":"\\\"baz\\\""})"_json;
  EXPECT_EQ(expected_response_body, actual_response_body);
}
} // namespace Envoy
