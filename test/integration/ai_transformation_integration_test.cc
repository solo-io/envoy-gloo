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
using json = nlohmann::json;

namespace Envoy {

const std::string DEFAULT_TRANSFORMATION =
R"EOF(
  request_transformation:
    ai_transformation: {}
)EOF";

const std::string FEATURES_TRANSFORMATION =
R"EOF(
  request_transformation:
    ai_transformation:
      enable_chat_streaming: true
      field_defaults:
        - field: max_tokens
          value: 100
      prompt_enrichment:
        prepend:
          - role: system
            content: you are a helpful assistant.
        append:
          - role: user
            content: I live in the US.
)EOF";

const std::string DEFAULT_FILTER_TRANSFORMATION =
R"EOF(
  {}
)EOF";

const std::string DEFAULT_MATCHER =
R"EOF(
  prefix: /
)EOF";

const std::string OPENAI_REQUEST_BODY =
R"EOF(
{
  "model": "gpt-4.1",
  "messages": [
    {
      "role": "developer",
      "content": "You are a helpful assistant."
    },
    {
      "role": "user",
      "content": "Hello!"
    }
  ]
}
)EOF";

const std::string OPENAI_RESPONSE_BODY =
R"EOF(
{
  "id": "chatcmpl-B9MBs8CjcvOU2jLn4n570S5qMJKcT",
  "object": "chat.completion",
  "created": 1741569952,
  "model": "gpt-4.1-2025-04-14",
  "choices": [
    {
      "index": 0,
      "message": {
        "role": "assistant",
        "content": "Hello! How can I assist you today?",
        "refusal": null,
        "annotations": []
      },
      "logprobs": null,
      "finish_reason": "stop"
    }
  ],
  "usage": {
    "prompt_tokens": 19,
    "completion_tokens": 10,
    "total_tokens": 29,
    "prompt_tokens_details": {
      "cached_tokens": 0,
      "audio_tokens": 0
    },
    "completion_tokens_details": {
      "reasoning_tokens": 0,
      "audio_tokens": 0,
      "accepted_prediction_tokens": 0,
      "rejected_prediction_tokens": 0
    }
  },
  "service_tier": "default"
}
)EOF";

const std::string ANTHROPIC_REQUEST_BODY =
R"EOF(
{
    "model": "claude-3-7-sonnet-20250219",
    "max_tokens": 1024,
    "messages": [
        {"role": "user", "content": "Hello, world"}
    ]
}
)EOF";

const std::string ANTHROPIC_RESPONSE_BODY =
R"EOF(
{
  "content": [
    {
      "text": "Hi! My name is Claude.",
      "type": "text"
    }
  ],
  "id": "msg_013Zva2CMHLNnXjNJJKqJ2EF",
  "model": "claude-3-7-sonnet-20250219",
  "role": "assistant",
  "stop_reason": "end_turn",
  "stop_sequence": null,
  "type": "message",
  "usage": {
    "input_tokens": 2095,
    "output_tokens": 503
  }
}
)EOF";

const std::string GEMINI_REQUEST_BODY =
R"EOF(
{
  "contents": [
    {
      "parts": [
        {
          "text": "How does AI work?"
        }
      ]
    }
  ]
}
)EOF";

const std::string GEMINI_RESPONSE_BODY =
R"EOF(
{
  "candidates": [
    {
      "content": {
        "parts": [
          {
            "text": "##  AI: A Simplified Explanation\n\nArtificial intelligence (AI) is a vast and complex field, but at its core, it's about creating systems that can **learn, reason, and act autonomously**.  Imagine teaching ...\n"
          }
        ],
        "role": "model"
      },
      "finishReason": "STOP",
      "index": 0,
      "safetyRatings": [
        {
          "category": "HARM_CATEGORY_SEXUALLY_EXPLICIT",
          "probability": "NEGLIGIBLE"
        },
        {
          "category": "HARM_CATEGORY_HATE_SPEECH",
          "probability": "NEGLIGIBLE"
        },
        {
          "category": "HARM_CATEGORY_HARASSMENT",
          "probability": "NEGLIGIBLE"
        },
        {
          "category": "HARM_CATEGORY_DANGEROUS_CONTENT",
          "probability": "NEGLIGIBLE"
        }
      ]
    }
  ],
  "usageMetadata": {
    "promptTokenCount": 4,
    "candidatesTokenCount": 568,
    "totalTokenCount": 572
  },
  "modelVersion": "gemini-1.5-flash-001"
}
)EOF";

// These tests are for testing various protocol combination at integration level and
// they are not covering all the features combinations. Features are test in the unit test
// extensively in test/extensions/filters/http/transformation/ai_transformer_test.cc
class AiTransformationIntegrationTest
  : public HttpProtocolIntegrationTest {
    public:
      AiTransformationIntegrationTest() : HttpProtocolIntegrationTest() {}
  /**
   * Initializer for an individual integration test.
   */
  void initialize() override {
    const std::string default_filter =
        loadListenerConfig(filter_transformation_string_, matcher_string_);

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
                [Extensions::HttpFilters::SoloHttpFilterNames::get().Transformation];
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
                      std::string_view body = "") {
    waitForNextUpstreamRequest();
    upstream_request_->encodeHeaders(
        Http::TestRequestHeaderMapImpl{{":status", "200"}}, body.empty());

    if (!body.empty()) {
      Buffer::OwnedImpl data(body);
      upstream_request_->encodeData(data, true);
    }

    ASSERT_TRUE(response->waitForEndStream());
  }

  ProtobufWkt::Struct azureEndPointMetaData() {
    static std::map<std::string, std::string> metadata{
      {"auth_token", "foobar"},
      {"json_schema", "openai"},
      {"provider", "azure"},
      {"model", "gpt-4o-mini"},
      {"path", "/openai/deployments/{{model}}/chat/completions?api-version=2024-02-15-preview"}
    };
    return MessageUtil::keyValueStruct(metadata);
  }

  ProtobufWkt::Struct openAiEndPointMetaData() {
    static std::map<std::string, std::string> metadata{
      {"auth_token", "foobar"},
      {"json_schema", "openai"},
      {"provider", "openai"},
      {"model", "o1-pro"},
      {"path", "/v1/chat/completions"}
    };
    return MessageUtil::keyValueStruct(metadata);
  }

  ProtobufWkt::Struct anthropicEndPointMetaData() {
    static std::map<std::string, std::string> metadata{
      {"auth_token", "foobar"},
      {"json_schema", "anthropic"},
      {"provider", "anthropic"},
      {"path", "/v1/chat/completions"}
    };
    return MessageUtil::keyValueStruct(metadata);
  }

  ProtobufWkt::Struct geminiEndPointMetaData() {
    static std::map<std::string, std::string> metadata{
      {"auth_token", "foobar"},
      {"json_schema", "gemini"},
      {"provider", "gemini"},
      {"model", "gemini-1.5-flash-001"},
      {"base_path", "/v1beta/models/{{model}}:"}
    };
    return MessageUtil::keyValueStruct(metadata);
  }

  ProtobufWkt::Struct vertexAiEndPointMetaData() {
    static std::map<std::string, std::string> metadata{
      {"auth_token", "foobar"},
      {"json_schema", "gemini"},
      {"provider", "vertexai"},
      {"model", "gemini-2"},
      {"base_path", "/vi/projects/my-project/locations/us-central/publishers/google/models/{{model}}:"}
    };
    return MessageUtil::keyValueStruct(metadata);
  }

  std::string_view getUpstreamHeaderValue(std::string_view key) {
    auto result = upstream_request_->headers().get(Http::LowerCaseString(key));

    if (result.empty()) {
      return "";
    }

    return result[0]->value().getStringView();
  }

  std::string transformation_string_{DEFAULT_TRANSFORMATION};
  std::string filter_transformation_string_{DEFAULT_FILTER_TRANSFORMATION};
  std::string matcher_string_{DEFAULT_MATCHER};
  bool downstream_filter_{false};
  ProtobufWkt::Struct *endpoint_metadata_{nullptr};

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
      [this](envoy::config::bootstrap::v3::Bootstrap& bootstrap) {
        if (!this->endpoint_metadata_) {
          return;
        }
        auto* static_resources = bootstrap.mutable_static_resources();
        for (int i = 0; i < static_resources->clusters_size(); ++i) {
          auto* cluster = static_resources->mutable_clusters(i);
          for (int j = 0; j < cluster->load_assignment().endpoints_size(); ++j) {
            auto* endpoint = cluster->mutable_load_assignment()->mutable_endpoints(j);
            for (int k = 0; k < endpoint->lb_endpoints_size(); ++k) {

              auto* lb_endpoint = endpoint->mutable_lb_endpoints(k);
              auto* metadata = lb_endpoint->mutable_metadata();
              (*metadata->mutable_filter_metadata())[Extensions::HttpFilters::SoloHttpFilterNames::get().Transformation].MergeFrom(*(this->endpoint_metadata_));
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

TEST_P(AiTransformationIntegrationTest, NoEndPointMetadata) {
  // No endpoint metadata (this should not happen but make sure we don't crash)
  // AI Transformer will bail early when there is no endpoint metadata at all
  // So, the request will be unchanged and just pass through
  initialize();
  std::string body = "hello world";
  std::string content_length = std::to_string(body.length());
  Http::TestRequestHeaderMapImpl request_headers{{":method", "POST"},
                                                 {":scheme", "http"},
                                                 {":authority", "solo.ai"},
                                                 {":path", "/whatever"},
                                                 {"content-length", content_length},
                                                };

  auto response = codec_client_->makeRequestWithBody(request_headers, body, true);
  processRequest(response, "");

  EXPECT_EQ("/whatever", getUpstreamHeaderValue(":path"));
  EXPECT_EQ(true, getUpstreamHeaderValue("authorization").empty());
  EXPECT_TRUE(response->complete());
  // Make sure body is not changed and passed through
  EXPECT_EQ(body, upstream_request_->body().toString());
  // Make sure content-length header is not removed and not changed
  EXPECT_EQ(content_length, getUpstreamHeaderValue("content-length"));

}

TEST_P(AiTransformationIntegrationTest, EmptyEndPointMetadata) {
  // There is endpoint metadata `io.solo.transformation` constains no fields
  // AI Transformer will fallback to the default which is openai
  ProtobufWkt::Struct metadata;
  endpoint_metadata_ = &metadata;
  initialize();
  Http::TestRequestHeaderMapImpl request_headers{{":method", "POST"},
                                                 {":scheme", "http"},
                                                 {":authority", "solo.ai"},
                                                 {":path", "/whatever"}};

  auto response = codec_client_->makeRequestWithBody(request_headers, OPENAI_REQUEST_BODY, true);
  processRequest(response, OPENAI_RESPONSE_BODY);

  EXPECT_EQ("/v1/chat/completions", getUpstreamHeaderValue(":path"));
  EXPECT_EQ(true, getUpstreamHeaderValue("authorization").empty());
  EXPECT_TRUE(response->complete());

}

TEST_P(AiTransformationIntegrationTest, WithAzureEndPointMetadata) {
  auto metadata = azureEndPointMetaData();
  endpoint_metadata_ = &metadata;
  initialize();
  Http::TestRequestHeaderMapImpl request_headers{{":method", "POST"},
                                                 {":scheme", "http"},
                                                 {":authority", "solo.ai"},
                                                 {":path", "/whatever"}};

  auto response = codec_client_->makeRequestWithBody(request_headers, OPENAI_REQUEST_BODY, true);
  processRequest(response, OPENAI_RESPONSE_BODY);

  EXPECT_EQ("/openai/deployments/gpt-4o-mini/chat/completions?api-version=2024-02-15-preview", getUpstreamHeaderValue(":path"));
  EXPECT_EQ("foobar", getUpstreamHeaderValue("api-key"));
  EXPECT_TRUE(response->complete());

}

TEST_P(AiTransformationIntegrationTest, WithOpenAiEndPointMetadata) {
  auto metadata = openAiEndPointMetaData();
  endpoint_metadata_ = &metadata;
  initialize();
  Http::TestRequestHeaderMapImpl request_headers{{":method", "POST"},
                                                 {":scheme", "http"},
                                                 {":authority", "solo.ai"},
                                                 {":path", "/whatever"}};

  auto response = codec_client_->makeRequestWithBody(request_headers, OPENAI_REQUEST_BODY, true);
  processRequest(response, OPENAI_RESPONSE_BODY);

  EXPECT_EQ("/v1/chat/completions", getUpstreamHeaderValue(":path"));
  EXPECT_EQ("Bearer foobar", getUpstreamHeaderValue("authorization"));
  EXPECT_TRUE(response->complete());

  auto parsed_original_body = json::parse(OPENAI_REQUEST_BODY);
  auto body = upstream_request_->body().toString();
  auto parsed_modified_body = json::parse(body);

  // The OpenAI Endpoint Metadata has model field set which will override the
  // model field in the body
  EXPECT_NE(parsed_original_body["model"], parsed_modified_body["model"]);
  EXPECT_EQ("o1-pro", parsed_modified_body["model"]);
}

TEST_P(AiTransformationIntegrationTest, WithAnthropicEndPointMetadata) {
  auto metadata = anthropicEndPointMetaData();
  endpoint_metadata_ = &metadata;
  initialize();
  Http::TestRequestHeaderMapImpl request_headers{{":method", "POST"},
                                                 {":scheme", "http"},
                                                 {":authority", "solo.ai"},
                                                 {":path", "/whatever"}};

  auto response = codec_client_->makeRequestWithBody(request_headers, ANTHROPIC_REQUEST_BODY, true);
  processRequest(response, ANTHROPIC_RESPONSE_BODY);

  EXPECT_EQ("/v1/chat/completions", getUpstreamHeaderValue(":path"));
  EXPECT_EQ("foobar", getUpstreamHeaderValue("x-api-key"));
  EXPECT_TRUE(response->complete());
}

TEST_P(AiTransformationIntegrationTest, WithGeminiEndPointMetadata) {
  auto metadata = geminiEndPointMetaData();
  endpoint_metadata_ = &metadata;
  initialize();
  Http::TestRequestHeaderMapImpl request_headers{{":method", "POST"},
                                                 {":scheme", "http"},
                                                 {":authority", "solo.ai"},
                                                 {":path", "/whatever"}};

  auto response = codec_client_->makeRequestWithBody(request_headers, GEMINI_REQUEST_BODY, true);
  processRequest(response, GEMINI_RESPONSE_BODY);

  EXPECT_EQ("/v1beta/models/gemini-1.5-flash-001:generateContent", getUpstreamHeaderValue(":path"));
  EXPECT_EQ("foobar", getUpstreamHeaderValue("x-goog-api-key"));
  EXPECT_TRUE(response->complete());
}

TEST_P(AiTransformationIntegrationTest, WithVertexAiEndPointMetadata) {
  auto metadata = vertexAiEndPointMetaData();
  endpoint_metadata_ = &metadata;
  initialize();
  Http::TestRequestHeaderMapImpl request_headers{{":method", "POST"},
                                                 {":scheme", "http"},
                                                 {":authority", "solo.ai"},
                                                 {":path", "/whatever"}};

  auto response = codec_client_->makeRequestWithBody(request_headers, GEMINI_REQUEST_BODY, true);
  processRequest(response, GEMINI_RESPONSE_BODY);

  EXPECT_EQ("/vi/projects/my-project/locations/us-central/publishers/google/models/gemini-2:generateContent",
    getUpstreamHeaderValue(":path"));
  EXPECT_EQ("Bearer foobar", getUpstreamHeaderValue("authorization"));
  EXPECT_TRUE(response->complete());
}

TEST_P(AiTransformationIntegrationTest, AnthropicWithAllFeaturesSet) {
  auto metadata = anthropicEndPointMetaData();
  endpoint_metadata_ = &metadata;
  transformation_string_ = FEATURES_TRANSFORMATION;
  initialize();
  Http::TestRequestHeaderMapImpl request_headers{{":method", "POST"},
                                                 {":scheme", "http"},
                                                 {":authority", "solo.ai"},
                                                 {":path", "/whatever"}};

  auto response = codec_client_->makeRequestWithBody(request_headers, ANTHROPIC_REQUEST_BODY, true);
  processRequest(response, ANTHROPIC_RESPONSE_BODY);
  EXPECT_TRUE(response->complete());

  auto body = upstream_request_->body().toString();
  auto parsed_modified_body = json::parse(body);

  auto expected_messages_json = json::parse(R"(
    [
      {"role": "user", "content": "Hello, world"},
      {"role": "user", "content": "I live in the US."}
    ]
  )");
  EXPECT_EQ(expected_messages_json, parsed_modified_body["messages"]);
  // The field_defaults setting does not have override on, so should have the existing value
  EXPECT_EQ(1024, parsed_modified_body["max_tokens"]);
  EXPECT_EQ(false, parsed_modified_body.contains("stream_options"));
  EXPECT_EQ(true, parsed_modified_body["stream"]);
  EXPECT_EQ("you are a helpful assistant.\n\n",parsed_modified_body["system"]);
}

TEST_P(AiTransformationIntegrationTest, OpenAiWithAllFeaturesSet) {
  auto metadata = openAiEndPointMetaData();
  endpoint_metadata_ = &metadata;
  transformation_string_ = FEATURES_TRANSFORMATION;
  initialize();
  Http::TestRequestHeaderMapImpl request_headers{{":method", "POST"},
                                                 {":scheme", "http"},
                                                 {":authority", "solo.ai"},
                                                 {":path", "/whatever"}};

  auto response = codec_client_->makeRequestWithBody(request_headers, OPENAI_REQUEST_BODY, true);
  processRequest(response, OPENAI_RESPONSE_BODY);
  EXPECT_TRUE(response->complete());

  auto body = upstream_request_->body().toString();
  auto parsed_modified_body = json::parse(body);

  auto expected_messages_json = json::parse(R"(
    [
      {"role": "system", "content": "you are a helpful assistant."},
      {"role": "developer", "content": "You are a helpful assistant."},
      {"role": "user", "content": "Hello!" },
      {"role": "user", "content": "I live in the US."}
    ]
  )");
  EXPECT_EQ(expected_messages_json, parsed_modified_body["messages"]);
  EXPECT_EQ(100, parsed_modified_body["max_tokens"]);
  auto expected_stream_options_json = json::parse(R"(
    {
      "include_usage": true
    }
  )");
  EXPECT_EQ(expected_stream_options_json, parsed_modified_body["stream_options"]);
  EXPECT_EQ(true, parsed_modified_body["stream"]);
}

TEST_P(AiTransformationIntegrationTest, GeminiWithAllFeaturesSet) {
  auto metadata = geminiEndPointMetaData();
  endpoint_metadata_ = &metadata;
  transformation_string_ = FEATURES_TRANSFORMATION;
  initialize();
  Http::TestRequestHeaderMapImpl request_headers{{":method", "POST"},
                                                 {":scheme", "http"},
                                                 {":authority", "solo.ai"},
                                                 {":path", "/whatever"}};

  auto response = codec_client_->makeRequestWithBody(request_headers, GEMINI_REQUEST_BODY, true);
  processRequest(response, GEMINI_RESPONSE_BODY);

  EXPECT_TRUE(response->complete());
  EXPECT_EQ("/v1beta/models/gemini-1.5-flash-001:streamGenerateContent?alt=sse", getUpstreamHeaderValue(":path"));
  auto body = upstream_request_->body().toString();
  auto parsed_modified_body = json::parse(body);
  auto expected_contents_json = json::parse(R"(
  [
    {"parts": [{"text": "How does AI work?" }]},
    {"role": "user", "parts": [{"text": "I live in the US."}]}
  ]
  )");
  auto expected_system_instruction_json = json::parse(R"(
  [
    {"role": "system", "parts": [{"text": "you are a helpful assistant."}]}
  ]
  )");
  EXPECT_EQ(expected_contents_json, parsed_modified_body["contents"]);
  EXPECT_EQ(expected_system_instruction_json, parsed_modified_body["system_instruction"]);
  EXPECT_EQ(100, parsed_modified_body["max_tokens"]);
}
} // namespace Envoy
