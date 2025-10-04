#include "source/extensions/filters/http/transformation/ai_transformer.h"

#include "test/mocks/common.h"
#include "test/mocks/http/mocks.h"
#include "test/mocks/server/mocks.h"
#include "test/mocks/upstream/mocks.h"

#include "absl/strings/str_cat.h"
#include "fmt/format.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::_;
using testing::AtLeast;
using testing::HasSubstr;
using testing::Invoke;
using testing::Return;
using testing::ReturnPointee;
using testing::ReturnRef;
using testing::SaveArg;
using testing::WithArg;

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

namespace {

using json = nlohmann::json;

TEST(FieldDefault, ValidJsonWithNumber) {
  const std::string yaml = R"(
    field: foobar
    value: 123
    override: true
  )";
  envoy::api::v2::filter::http::FieldDefault fDefault;
  TestUtility::loadFromYaml(yaml, fDefault);
  Transformation::FieldDefault fieldDefault(fDefault);
  EXPECT_EQ(std::string{"foobar"}, fieldDefault.field());
  EXPECT_EQ(123, fieldDefault.value());
  EXPECT_NE("123", fieldDefault.value());
}

TEST(FieldDefault, ValidJsonWithJsonObject) {
  const std::string yaml = R"(
    field: foobar
    value: {"number": 123, "test": ["a", "b", "c"]}
    override: true
  )";
  envoy::api::v2::filter::http::FieldDefault fDefault;
  TestUtility::loadFromYaml(yaml, fDefault);
  Transformation::FieldDefault fieldDefault(fDefault);
  EXPECT_EQ(std::string{"foobar"}, fieldDefault.field());
  EXPECT_EQ(true, fieldDefault.override());
  auto &v = fieldDefault.value(); // v is a const &
  ASSERT_EQ(true, v["test"].is_array());
  EXPECT_EQ(std::string{"a"}, v["test"][0]);
  EXPECT_EQ(std::string{"b"}, v["test"][1]);
  EXPECT_EQ(std::string{"c"}, v["test"][2]);
  EXPECT_EQ(123, v["number"]);
  EXPECT_NE("123", v["number"]);
  EXPECT_NE(std::string{"123"}, v["number"]);

  EXPECT_EQ(true, v.contains("number"));
  EXPECT_EQ(true, v.contains("test"));
  EXPECT_EQ(false, v.contains("notthere"));
  EXPECT_EQ(2, v.size());
}

TEST(PromptEnrichment, ValidJson) {
  const std::string yaml = R"(
    append:
      - role: developer
        content: response with coding analogy
    prepend:
      - role: system
        content: speak in British accent
      - role: user
        content: my name is James Bond
  )";

  envoy::api::v2::filter::http::PromptEnrichment pe;
  TestUtility::loadFromYaml(yaml, pe);
  Transformation::PromptEnrichment promptEnrichment(pe);
  auto &promptsToPrepend = promptEnrichment.prepend();
  ASSERT_EQ(2, promptsToPrepend.size());
  EXPECT_STREQ("system", promptsToPrepend[0].role().c_str());
  EXPECT_STREQ("speak in British accent", promptsToPrepend[0].content().c_str());
  EXPECT_STREQ("user", promptsToPrepend[1].role().c_str());
  EXPECT_STREQ("my name is James Bond", promptsToPrepend[1].content().c_str());

  auto &promptsToAppend = promptEnrichment.append();
  ASSERT_EQ(1, promptsToAppend.size());
  EXPECT_STREQ("developer", promptsToAppend[0].role().c_str());
  EXPECT_STREQ("response with coding analogy", promptsToAppend[0].content().c_str());

}

class AiTransformerTest : public ::testing::Test {
  protected:

  const std::string AI_STREAMING_DISABLED {
    R"(
      enable_chat_streaming: false
    )",
  };
  const std::string AI_STREAMING_ENABLED {
    R"(
      enable_chat_streaming: true
    )",
  };
  const std::string AI_FIELD_DEFAULTS {
    R"(
      field_defaults:
        - field: foo
          value: { "bar": "foobar", "test": 123.0 }
        - field: max_tokens
          value: 100
        - field: temperature
          value: 0.7
          override: true
    )",
  };
  const std::string GEMINI_FIELD_DEFAULTS {
    R"(
      field_defaults:
        - field: generationConfig
          value: {"stopSequences": [ "foobar" ], "temperature": "0.2", "maxOutputTokens": 400}
          override: true
        - field: foo
          value: bar
    )",
  };
  const std::string AI_PROMPT_ENRICHMENT {
    R"(
      prompt_enrichment:
        prepend:
          - role: system
            content: you are a helpful assistant.
          - role: developer
            content: reply everything with programming analogy.
          - role: user
            content: my name is Bond.
        append:
          - role: developer
            content: you are expert in assembly language.
          - role: user
            content: I live in the US.
          - role: system
            content: reply in British accent.
    )",
  };

  const std::string AZURE_OPENAI_UPSTREAM_METADATA {
    R"(
      {
        "filter_metadata": {
        "io.solo.transformation": {
          "auth_token": "foobar",
          "json_schema": "openai",
          "provider": "azure",
          "model": "gpt-4o-mini",
          "path": "/openai/deployments/{{model}}/chat/completions?api-version=2024-02-15-preview"
        }
        }
      }
    )"
  };
  const std::string OPENAI_UPSTREAM_METADATA {
    R"(
      {
        "filter_metadata": {
        "io.solo.transformation": {
          "auth_token": "foobar",
          "json_schema": "openai",
          "provider": "openai",
          "model": "gpt-4o-mini",
          "path": "/v1/chat/completions"
        }
        }
      }
    )"
  };
  const std::string ANTHROPIC_UPSTREAM_METADATA {
    R"(
      {
        "filter_metadata": {
        "io.solo.transformation": {
          "auth_token": "foobar",
          "json_schema": "anthropic",
          "provider": "anthropic",
          "path": "/v1/messages"
        }
        }
      }
    )"
  };
  const std::string GEMINI_UPSTREAM_METADATA {
    R"(
      {
        "filter_metadata": {
        "io.solo.transformation": {
          "auth_token": "foobar",
          "json_schema": "gemini",
          "provider": "gemini",
          "model": "gemini-1.5-flash-001",
          "base_path": "/v1beta/models/{{model}}:"
        }
        }
      }
    )"
  };
  const std::string VERTEXAI_UPSTREAM_METADATA {
    R"(
      {
        "filter_metadata": {
        "io.solo.transformation": {
          "auth_token": "foobar",
          "json_schema": "gemini",
          "provider": "vertexai",
          "model": "gemini-2",
          "base_path": "/vi/projects/my-project/locations/us-central/publishers/google/models/{{model}}:"
        }
        }
      }
    )"
  };
  const std::string BEDROCK_UPSTREAM_METADATA {
    R"(
      {
        "filter_metadata": {
        "io.solo.transformation": {
          "json_schema": "bedrock",
          "provider": "bedrock",
          "model": "anthropic.claude-3-5-haiku-20241022-v1:0",
          "base_path": "/model/{{model}}/"
        }
        }
      }
    )"
  };
  class MockUpstreamStreamFilterCallbacks : public Http::UpstreamStreamFilterCallbacks {
  public:
    ~MockUpstreamStreamFilterCallbacks() override = default;

    MOCK_METHOD(StreamInfo::StreamInfo&, upstreamStreamInfo, ());
    MOCK_METHOD(OptRef<Router::GenericUpstream>, upstream, ());
    MOCK_METHOD(void, dumpState, (std::ostream& os, int indent_level), (const));
    MOCK_METHOD(bool, pausedForConnect, (), (const));
    MOCK_METHOD(void, setPausedForConnect, (bool value));
    MOCK_METHOD(bool, pausedForWebsocketUpgrade, (), (const));
    MOCK_METHOD(void, setPausedForWebsocketUpgrade, (bool value));
    MOCK_METHOD(const Http::ConnectionPool::Instance::StreamOptions&, upstreamStreamOptions, (), (const));
    MOCK_METHOD(void, addUpstreamCallbacks, (Http::UpstreamCallbacks& callbacks));
    MOCK_METHOD(void, setUpstreamToDownstream, (Router::UpstreamToDownstream& upstream_to_downstream_interface));
    MOCK_METHOD(void, setupRouteTimeoutForWebsocketUpgrade, ());
    MOCK_METHOD(void, disableRouteTimeoutForWebsocketUpgrade, ());
    MOCK_METHOD(void, disablePerTryTimeoutForWebsocketUpgrade, ());

  };

  std::shared_ptr<AiTransformer> createAiTransformer(
    const std::string &ai_transformation_yaml,
    const std::string &metadata_json,
    bool log_request_response_info = false
  ) {
    auto metadata = new envoy::config::core::v3::Metadata();
    TestUtility::loadFromJson(metadata_json, *metadata);
    metadata_.reset(metadata);

    auto host_description = new NiceMock<Upstream::MockHostDescription>();
    ON_CALL(*host_description, metadata()).WillByDefault(Return(metadata_));
    Upstream::HostDescriptionConstSharedPtr host_ptr(host_description);
    stream_info_.upstreamInfo()->setUpstreamHost(host_ptr);
    ON_CALL(filter_callbacks_, upstreamCallbacks())
      .WillByDefault(Return(OptRef<Http::UpstreamStreamFilterCallbacks>{upstream_callbacks_}));
    ON_CALL(upstream_callbacks_, upstreamStreamInfo()).WillByDefault(ReturnRef(stream_info_));

    envoy::api::v2::filter::http::AiTransformation aiTransformation;
    TestUtility::loadFromYaml(ai_transformation_yaml, aiTransformation);
    google::protobuf::BoolValue val;
    val.set_value(log_request_response_info);
    return std::make_shared<AiTransformer>(aiTransformation, val);
  }

  std::string_view getPath() {
    if (!headers_.Path()) {
      return "";
    }
    return headers_.Path()->value().getStringView();
  }

  void setPath(std::string_view path) {
    headers_.setPath(path);
  }

  std::string_view getHeaderValue(std::string_view key) {
    auto result = headers_.get(Http::LowerCaseString(key));

    if (result.empty()) {
      return "";
    }

    return result[0]->value().getStringView();
  }

  void setHeaderValue(std::string_view key, std::string_view value) {
    headers_.setCopy(Http::LowerCaseString(key), value);
  }

  void setBody(std::string_view body) {
    body_.drain(body_.length());
    body_.add(body);
  }

  std::string removeAuthTokenFromMetadata(const std::string &metadata) {
    auto data = json::parse(metadata);
    data["filter_metadata"]["io.solo.transformation"].erase("auth_token");

    return data.dump();
  }

  NiceMock<Server::Configuration::MockFactoryContext> context_;
  NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_;
  NiceMock<MockUpstreamStreamFilterCallbacks> upstream_callbacks_;
  NiceMock<Envoy::StreamInfo::MockStreamInfo> stream_info_;
  Upstream::MetadataConstSharedPtr metadata_;

  Http::TestRequestHeaderMapImpl headers_;
  Buffer::OwnedImpl body_{"hello world"};
};

TEST_F(AiTransformerTest, AzurePathAndAuthTransformation) {
  auto aiTransformer = createAiTransformer(
    AI_STREAMING_DISABLED,
    AZURE_OPENAI_UPSTREAM_METADATA
  );

  setPath("/whatever?foo=bar"); // We don't preserve the qs param when re-writing the path
  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  EXPECT_EQ(std::string{
    "/openai/deployments/gpt-4o-mini/chat/completions?api-version=2024-02-15-preview"
    }, getPath());

  EXPECT_EQ(getHeaderValue("api-key"), "foobar");

  // Test auth token pass through
  aiTransformer = createAiTransformer(
    AI_STREAMING_DISABLED,
    removeAuthTokenFromMetadata(AZURE_OPENAI_UPSTREAM_METADATA)
  );
  headers_.clear();
  setPath("/whatever");
  setHeaderValue("authorization", "Bearer 12345");
  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  EXPECT_EQ(getHeaderValue("api-key"), "12345");

  headers_.clear();
  setPath("/whatever");
  setHeaderValue("api-key", "67890");
  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  EXPECT_EQ(getHeaderValue("api-key"), "67890");

  // Test auth token pass through and both authorization and the key header already exists
  headers_.clear();
  setPath("/whatever");
  setHeaderValue("authorization", "Bearer 12345");
  setHeaderValue("api-key", "67890");
  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  EXPECT_EQ(getHeaderValue("api-key"), "67890");
}

TEST_F(AiTransformerTest, AzureFieldDefaultsTransformation) {
  auto aiTransformer = createAiTransformer(
    AI_FIELD_DEFAULTS,
    AZURE_OPENAI_UPSTREAM_METADATA
  );

  // Make sure we pass through even the body is not json
  setPath("/whatever?foo=bar"); // We don't preserve the qs param when re-writing the path
  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  EXPECT_EQ(std::string{"hello world"}, body_.toString());

  // valid json body with just empty object
  setBody("{}");
  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  auto parsed_body = json::parse(body_.toString());
  ASSERT_EQ(false, parsed_body.empty());
  EXPECT_EQ("0.7", parsed_body["temperature"]) << "\njson: " << parsed_body.dump() << "\nstr: " << body_.toString();
  EXPECT_EQ(100, parsed_body["max_tokens"]) << "json: " << parsed_body.dump() << "\nstr: " << body_.toString();
  auto expected_json = json::parse(R"({ "bar": "foobar", "test": "123.0" })");
  EXPECT_EQ(expected_json, parsed_body["foo"]) << "json: " << parsed_body.dump() << "\nstr: " << body_.toString();

  // Test override
  setBody(R"(
  {
    "model": "gpt-4o",
    "messages": [
      { "role": "user", "content": "Hello!" }
    ],
    "temperature": "0.1",
    "max_tokens": 5
  }
  )");

  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  parsed_body = json::parse(body_.toString());
  // temperature has override set to true
  EXPECT_EQ("0.7", parsed_body["temperature"]) << "\njson: " << parsed_body.dump() << "\nstr: " << body_.toString();
  // max_tokens doesn't have override set to true
  EXPECT_EQ(5, parsed_body["max_tokens"]) << "\njson: " << parsed_body.dump() << "\nstr: " << body_.toString();
  // model is NOT changed even it's set in the upstream metadata because the model field
  // for Azure OpenAI actually is ignored as the model is in the url path
  EXPECT_EQ(std::string{"gpt-4o"}, parsed_body["model"]);
  // message is not changed
  expected_json = json::parse(R"([ {"role": "user", "content": "Hello!"} ])");
  EXPECT_EQ(expected_json, parsed_body["messages"]);

}

TEST_F(AiTransformerTest, AzurePromptEnrichmentTransformation) {
  auto aiTransformer = createAiTransformer(
    AI_PROMPT_ENRICHMENT,
    AZURE_OPENAI_UPSTREAM_METADATA
  );

  setBody(R"(
  {
    "messages": [
      { "role": "user", "content": "Hello!" }
    ]
  }
  )");

  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  auto parsed_body = json::parse(body_.toString());
  auto expected_json = json::parse(R"([
    {"role": "system", "content": "you are a helpful assistant."},
    {"role": "developer", "content": "reply everything with programming analogy."},
    {"role": "user", "content": "my name is Bond."},
    {"role": "user", "content": "Hello!"},
    {"role": "developer", "content": "you are expert in assembly language."},
    {"role": "user", "content": "I live in the US."},
    {"role": "system", "content": "reply in British accent."}
  ])");
  EXPECT_EQ(expected_json, parsed_body["messages"]) << "\nbody: " << body_.toString();

}

TEST_F(AiTransformerTest, AnthropicPathAndAuthTransformation) {
  auto aiTransformer = createAiTransformer(
    AI_STREAMING_DISABLED,
    ANTHROPIC_UPSTREAM_METADATA
  );

  setPath("/whatever?foo=bar"); // We don't preserve the qs param when re-writing the path
  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  EXPECT_EQ(std::string{"/v1/messages"}, getPath());

  EXPECT_EQ(getHeaderValue("x-api-key"), "foobar");

  // Test auth token pass through
  aiTransformer = createAiTransformer(
    AI_STREAMING_DISABLED,
    removeAuthTokenFromMetadata(ANTHROPIC_UPSTREAM_METADATA)
  );

  headers_.clear();
  setPath("/whatever");
  setHeaderValue("authorization", "Bearer 12345");
  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  EXPECT_EQ(getHeaderValue("x-api-key"), "12345");

  headers_.clear();
  setPath("/whatever");
  setHeaderValue("x-api-key", "67890");
  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  EXPECT_EQ(getHeaderValue("x-api-key"), "67890");

  // Test auth token pass through and both authorization and the key header already exists
  headers_.clear();
  setPath("/whatever");
  setHeaderValue("authorization", "Bearer 12345");
  setHeaderValue("x-api-key", "67890");
  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  EXPECT_EQ(getHeaderValue("x-api-key"), "67890");
}

TEST_F(AiTransformerTest, AnthropicEnableChatStreaming) {
  auto aiTransformer = createAiTransformer(
    AI_STREAMING_ENABLED,
    ANTHROPIC_UPSTREAM_METADATA
  );

  setPath("/whatever");
  setBody(R"(
  {
    "model": "claude-3-7-sonnet-20250219",
    "temperature": "0.1",
    "max_tokens": 1024,
    "messages": [
        {"role": "user", "content": "Hello!"}
    ]
  }
  )");

  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  auto parsed_body = json::parse(body_.toString());
  EXPECT_EQ(true, parsed_body["stream"]) << parsed_body.dump();
  EXPECT_EQ(false, parsed_body.contains("stream_options"));
}

TEST_F(AiTransformerTest, AnthropicFieldDefaultsTransformation) {
  auto aiTransformer = createAiTransformer(
    AI_FIELD_DEFAULTS,
    ANTHROPIC_UPSTREAM_METADATA
  );

  // Make sure we pass through even the body is not json
  setPath("/whatever?foo=bar"); // We don't preserve the qs param when re-writing the path
  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  EXPECT_EQ(std::string{"hello world"}, body_.toString());

  // valid json body with just empty object
  setBody("{}");
  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  auto parsed_body = json::parse(body_.toString());
  ASSERT_EQ(false, parsed_body.empty());
  EXPECT_EQ("0.7", parsed_body["temperature"]) << "\njson: " << parsed_body.dump() << "\nstr: " << body_.toString();
  EXPECT_EQ(100, parsed_body["max_tokens"]) << "json: " << parsed_body.dump() << "\nstr: " << body_.toString();
  auto expected_json = json::parse(R"({ "bar": "foobar", "test": "123.0" })");
  EXPECT_EQ(expected_json, parsed_body["foo"]) << "json: " << parsed_body.dump() << "\nstr: " << body_.toString();

  // Test override
  setBody(R"(
  {
    "model": "claude-3-7-sonnet-20250219",
    "temperature": "0.1",
    "max_tokens": 1024,
    "messages": [
        {"role": "user", "content": "Hello!"}
    ]
  }
  )");

  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  parsed_body = json::parse(body_.toString());
  // temperature has override set to true
  EXPECT_EQ("0.7", parsed_body["temperature"]) << "\njson: " << parsed_body.dump() << "\nstr: " << body_.toString();
  // max_tokens doesn't have override set to true
  EXPECT_EQ(1024, parsed_body["max_tokens"]) << "\njson: " << parsed_body.dump() << "\nstr: " << body_.toString();
  // model is NOT changed because it's not set in the upstream
  EXPECT_EQ(std::string{"claude-3-7-sonnet-20250219"}, parsed_body["model"]);
  // message is not changed
  expected_json = json::parse(R"([ {"role": "user", "content": "Hello!"} ])");
  EXPECT_EQ(expected_json, parsed_body["messages"]);

}

TEST_F(AiTransformerTest, AnthropicPromptEnrichmentTransformation) {
  auto aiTransformer = createAiTransformer(
    AI_PROMPT_ENRICHMENT,
    ANTHROPIC_UPSTREAM_METADATA
  );

  setBody(R"(
  {
    "model": "claude-3-7-sonnet-20250219",
    "max_tokens": 1024,
    "messages": [
        {"role": "user", "content": "Hello!"}
    ]
  }
  )");

  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  auto parsed_body = json::parse(body_.toString());
  std::string system_prompt = absl::StrCat(
                  std::string{"you are a helpful assistant.\n"},
                  std::string{"reply in British accent.\n"}
                );
  std::string developer_prompt = absl::StrCat(
                  "reply everything with programming analogy.\n",
                  "you are expert in assembly language.\n"
                );
  auto expected_json = json::parse(R"([
    {"role": "user", "content": "my name is Bond."},
    {"role": "user", "content": "Hello!"},
    {"role": "user", "content": "I live in the US."}
  ])");
  EXPECT_EQ(expected_json, parsed_body["messages"]) << "\nbody: " << body_.toString();
  // Anthropic has a single filed for the system/developer prompt
  std::string expected_system_prompt = absl::StrCat(system_prompt, "\n", developer_prompt);
  EXPECT_EQ(expected_system_prompt, parsed_body["system"]) << "\nbody: " << body_.toString();
}

TEST_F(AiTransformerTest, OpenAIPathAndAuthTransformation) {
  auto aiTransformer = createAiTransformer(
    AI_STREAMING_DISABLED,
    OPENAI_UPSTREAM_METADATA
  );

  setPath("/whatever?foo=bar"); // We don't preserve the qs param when re-writing the path
  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  EXPECT_EQ(std::string{"/v1/chat/completions"}, getPath());

  EXPECT_EQ(getHeaderValue("authorization"), "Bearer foobar");

  // Test auth token pass through
  headers_.clear();
  setPath("/whatever");
  setHeaderValue("authorization", "Bearer 12345");
  aiTransformer = createAiTransformer(
    AI_STREAMING_DISABLED,
    removeAuthTokenFromMetadata(OPENAI_UPSTREAM_METADATA)
  );
  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  EXPECT_EQ(getHeaderValue("authorization"), "Bearer 12345");

  // For OpenAI, we will detect if it's one of the platform API endpoint and only strip off any prefixes
  setPath("/v1/models?foo=bar"); // We do preserve the qs param when in these cases
  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  EXPECT_EQ(std::string{"/v1/models?foo=bar"}, getPath());

  setPath("/whatever/bah/bah/bah/v1/audio");
  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  EXPECT_EQ(std::string{"/v1/audio"}, getPath());

  setPath("/whatever/v1/embeddings?foo=bar");
  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  EXPECT_EQ(std::string{"/v1/embeddings?foo=bar"}, getPath());

  setPath("/whatever/v1/fine_tuning?foo=bar");
  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  EXPECT_EQ(std::string{"/v1/fine_tuning?foo=bar"}, getPath());

  setPath("/v1/files?foo=bar");
  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  EXPECT_EQ(std::string{"/v1/files?foo=bar"}, getPath());

  setPath("/v1/uploads/1");
  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  EXPECT_EQ(std::string{"/v1/uploads/1"}, getPath());

  setPath("/openai/v1/images");
  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  EXPECT_EQ(std::string{"/v1/images"}, getPath());

  setPath("/openai/v1beta/moderations");
  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  EXPECT_EQ(std::string{"/v1beta/moderations"}, getPath());

}

TEST_F(AiTransformerTest, OpenAIEnableChatStreaming) {
  auto aiTransformer = createAiTransformer(
    AI_STREAMING_ENABLED,
    OPENAI_UPSTREAM_METADATA
  );

  setPath("/whatever");
  setBody(R"(
  {
    "model": "gpt-4o",
    "messages": [
      {
        "role": "user",
        "content": "Hello!"
      }
    ],
    "temperature": "0.1",
    "max_tokens": 5
  }
  )");

  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  auto parsed_body = json::parse(body_.toString());
  EXPECT_EQ(true, parsed_body["stream"]);
  auto expected_stream_options_json = json::parse(R"(
    {
      "include_usage": true
    }
  )");
  EXPECT_EQ(expected_stream_options_json, parsed_body["stream_options"]);

  // With Existing stream options
  setBody(R"(
  {
    "model": "gpt-4o",
    "messages": [
      {
        "role": "user",
        "content": "Hello!"
      }
    ],
    "temperature": "0.1",
    "max_tokens": 5,
    "stream": false,
    "stream_options": { "foo": "bar" }
  }
  )");
  headers_.clear();
  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  parsed_body = json::parse(body_.toString());
  EXPECT_EQ(true, parsed_body["stream"]);
  expected_stream_options_json = json::parse(R"(
    {
      "foo": "bar",
      "include_usage": true
    }
  )");
  EXPECT_EQ(expected_stream_options_json, parsed_body["stream_options"]);
}

TEST_F(AiTransformerTest, OpenAIFieldDefaultsTransformation) {
  auto aiTransformer = createAiTransformer(
    AI_FIELD_DEFAULTS,
    OPENAI_UPSTREAM_METADATA
  );

  // Make sure we pass through even the body is not json
  setPath("/whatever?foo=bar"); // We don't preserve the qs param when re-writing the path
  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  EXPECT_EQ(std::string{"hello world"}, body_.toString());

  // valid json body with just empty object
  setBody("{}");
  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  auto parsed_body = json::parse(body_.toString());
  ASSERT_EQ(false, parsed_body.empty());
  EXPECT_EQ("0.7", parsed_body["temperature"]) << "\njson: " << parsed_body.dump() << "\nstr: " << body_.toString();
  EXPECT_EQ(100, parsed_body["max_tokens"]) << "json: " << parsed_body.dump() << "\nstr: " << body_.toString();
  auto expected_json = json::parse(R"({ "bar": "foobar", "test": "123.0" })");
  EXPECT_EQ(expected_json, parsed_body["foo"]) << "json: " << parsed_body.dump() << "\nstr: " << body_.toString();

  // Test override
  setBody(R"(
  {
    "model": "gpt-4o",
    "messages": [
      {
        "role": "user",
        "content": "Hello!"
      }
    ],
    "temperature": "0.1",
    "max_tokens": 5
  }
  )");

  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  parsed_body = json::parse(body_.toString());
  // temperature has override set to true
  EXPECT_EQ("0.7", parsed_body["temperature"]) << "\njson: " << parsed_body.dump() << "\nstr: " << body_.toString();
  // max_tokens doesn't have override set to true
  EXPECT_EQ(5, parsed_body["max_tokens"]) << "\njson: " << parsed_body.dump() << "\nstr: " << body_.toString();
  // model is changed to the one specified in the upstream metadata
  EXPECT_EQ(std::string{"gpt-4o-mini"}, parsed_body["model"]);
  // message is not changed
  expected_json = json::parse(R"(
    [
      {
        "role": "user",
        "content": "Hello!"
      }
    ]
  )");
  EXPECT_EQ(expected_json, parsed_body["messages"]);

}

TEST_F(AiTransformerTest, OpenAIPromptEnrichmentTransformation) {
  auto aiTransformer = createAiTransformer(
    AI_PROMPT_ENRICHMENT,
    OPENAI_UPSTREAM_METADATA
  );

  setBody(R"(
  {
    "model": "gpt-4o-mini",
    "messages": [
      { "role": "user", "content": "Hello!" }
    ]
  }
  )");

  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  auto parsed_body = json::parse(body_.toString());
  auto expected_json = json::parse(R"([
    {"role": "system", "content": "you are a helpful assistant."},
    {"role": "developer", "content": "reply everything with programming analogy."},
    {"role": "user", "content": "my name is Bond."},
    {"role": "user", "content": "Hello!"},
    {"role": "developer", "content": "you are expert in assembly language."},
    {"role": "user", "content": "I live in the US."},
    {"role": "system", "content": "reply in British accent."}
  ])");
  EXPECT_EQ(expected_json, parsed_body["messages"]) << "\nbody: " << body_.toString();

}

TEST_F(AiTransformerTest, GeminiPathAndAuthTransformation) {
  auto aiTransformer = createAiTransformer(
    AI_STREAMING_DISABLED,
    GEMINI_UPSTREAM_METADATA
  );

  setPath("/whatever?foo=bar"); // We don't preserve the qs param when re-writing the path
  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  EXPECT_EQ(std::string{
    "/v1beta/models/gemini-1.5-flash-001:generateContent"
    }, getPath());

  EXPECT_EQ(getHeaderValue("x-goog-api-key"), "foobar");

  // Test auth token pass through
  aiTransformer = createAiTransformer(
    AI_STREAMING_DISABLED,
    removeAuthTokenFromMetadata(GEMINI_UPSTREAM_METADATA)
  );
  headers_.clear();
  setPath("/whatever");
  setHeaderValue("authorization", "Bearer 12345");
  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  EXPECT_EQ(getHeaderValue("x-goog-api-key"), "12345");

  headers_.clear();
  setPath("/whatever");
  setHeaderValue("x-goog-api-key", "67890");
  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  EXPECT_EQ(getHeaderValue("x-goog-api-key"), "67890");

  // Test auth token pass through and both authorization and the key header already exists
  headers_.clear();
  setPath("/whatever");
  setHeaderValue("authorization", "Bearer 12345");
  setHeaderValue("x-goog-api-key", "67890");
  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  EXPECT_EQ(getHeaderValue("x-goog-api-key"), "67890");

  // check path for streaming
  aiTransformer = createAiTransformer(
    AI_STREAMING_ENABLED,
    GEMINI_UPSTREAM_METADATA
  );
  setPath("/whatever?foo=bar"); // We don't preserve the qs param when re-writing the path
  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  EXPECT_EQ(std::string{
    "/v1beta/models/gemini-1.5-flash-001:streamGenerateContent?alt=sse"
    }, getPath());
}

TEST_F(AiTransformerTest, GeminiFieldDefaultsTransformation) {
  auto aiTransformer = createAiTransformer(
    GEMINI_FIELD_DEFAULTS,
    GEMINI_UPSTREAM_METADATA
  );

  // Make sure we pass through even the body is not json
  setPath("/whatever?foo=bar"); // We don't preserve the qs param when re-writing the path
  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  EXPECT_EQ(std::string{"hello world"}, body_.toString());

  // valid json body with just empty object
  setBody("{}");
  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  auto parsed_body = json::parse(body_.toString());
  ASSERT_EQ(false, parsed_body.empty());
  EXPECT_EQ(std::string{"bar"}, parsed_body["foo"]) << "json: " << parsed_body.dump() << "\nstr: " << body_.toString();
  auto expected_json = json::parse(R"({"stopSequences": [ "foobar" ], "temperature": "0.2", "maxOutputTokens": 400})");
  EXPECT_EQ(expected_json, parsed_body["generationConfig"]) << "json: " << parsed_body.dump() << "\nstr: " << body_.toString();

  // Test override
  setBody(R"(
  {
    "contents": [
      {"role":"user", "parts":[{ "text": "Hello"}]}
    ],
    "generationConfig": {
      "temperature": "1.0",
      "maxOutputTokens": 800,
      "topP": "0.8",
      "topK": 10
    }
  }
  )");

  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  parsed_body = json::parse(body_.toString());
  // no model is set
  EXPECT_EQ(false, parsed_body.contains("model"));
  // We don't merge the json, so the entire generationConfig field is replaced
  EXPECT_EQ(expected_json, parsed_body["generationConfig"]) << "json: " << parsed_body.dump() << "\nstr: " << body_.toString();
  // contents is not changed
  expected_json = json::parse(R"([ {"role":"user", "parts":[{ "text": "Hello"}]} ])");
  EXPECT_EQ(expected_json, parsed_body["contents"]);

}

TEST_F(AiTransformerTest, GeminiPromptEnrichmentTransformation) {
  auto aiTransformer = createAiTransformer(
    AI_PROMPT_ENRICHMENT,
    GEMINI_UPSTREAM_METADATA
  );

  setBody(R"(
  {
    "contents": [
      {"role":"user", "parts":[{ "text": "Hello!"}]}
    ]
  }
  )");

  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  auto parsed_body = json::parse(body_.toString());
  auto expected_system_instruction_json = json::parse(R"(
  [
    {"role": "system", "parts": [{"text": "you are a helpful assistant."}] },
    {"role": "developer", "parts": [{"text": "reply everything with programming analogy."}] },
    {"role": "developer", "parts": [{"text": "you are expert in assembly language."}] },
    {"role": "system", "parts": [{"text": "reply in British accent."}] }
  ]
  )");
  auto expected_contents_json = json::parse(R"(
  [
    {"role": "user", "parts": [{"text": "my name is Bond."}] },
    {"role": "user", "parts": [{"text": "Hello!"}] },
    {"role": "user", "parts": [{"text": "I live in the US."}] }
  ]
  )");
  EXPECT_EQ(expected_contents_json, parsed_body["contents"]) << "\nbody: " << body_.toString();
  EXPECT_EQ(expected_system_instruction_json, parsed_body["system_instruction"]) << "\nbody: " << body_.toString();

  // Test with existing system_instruction
  setBody(R"(
  {
    "system_instruction": [
      {"parts":[{ "text": "You name is Bob."}]}
    ],
    "contents": [
      {"role":"user", "parts":[{ "text": "Hello!"}]}
    ]
  }
  )");
  headers_.clear();
  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  parsed_body = json::parse(body_.toString());
  expected_system_instruction_json = json::parse(R"(
  [
    {"role": "system", "parts": [{"text": "you are a helpful assistant."}] },
    {"role": "developer", "parts": [{"text": "reply everything with programming analogy."}] },
    {"parts":[{ "text": "You name is Bob."}]},
    {"role": "developer", "parts": [{"text": "you are expert in assembly language."}] },
    {"role": "system", "parts": [{"text": "reply in British accent."}] }
  ]
  )");
  EXPECT_EQ(expected_contents_json, parsed_body["contents"]) << "\nbody: " << body_.toString();
  EXPECT_EQ(expected_system_instruction_json, parsed_body["system_instruction"]) << "\nbody: " << body_.toString();
}

TEST_F(AiTransformerTest, VertexAIPathAndAuthTransformation) {
  auto aiTransformer = createAiTransformer(
    AI_STREAMING_DISABLED,
    VERTEXAI_UPSTREAM_METADATA
  );

  setPath("/whatever?foo=bar"); // We don't preserve the qs param when re-writing the path
  Buffer::OwnedImpl body("hello world");
  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  EXPECT_EQ(std::string{
    "/vi/projects/my-project/locations/us-central/publishers/google/models/gemini-2:generateContent"
    }, getPath());

  EXPECT_EQ(getHeaderValue("authorization"), "Bearer foobar");

  // Test auth token pass through
  headers_.clear();
  setPath("/whatever");
  setHeaderValue("authorization", "Bearer 12345");
  aiTransformer = createAiTransformer(
    AI_STREAMING_DISABLED,
    removeAuthTokenFromMetadata(VERTEXAI_UPSTREAM_METADATA)
  );
  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  EXPECT_EQ(getHeaderValue("authorization"), "Bearer 12345");

  // check path for streaming
  aiTransformer = createAiTransformer(
    AI_STREAMING_ENABLED,
    VERTEXAI_UPSTREAM_METADATA
  );
  setPath("/whatever?foo=bar"); // We don't preserve the qs param when re-writing the path
  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  EXPECT_EQ(std::string{
    "/vi/projects/my-project/locations/us-central/publishers/google/models/gemini-2:streamGenerateContent?alt=sse"
    }, getPath());
}

TEST_F(AiTransformerTest, VertexAIFieldDefaultsTransformation) {
  // This test is exactly the same as GeminiFieldDefaultsTransformation exact the upstream metadata
  auto aiTransformer = createAiTransformer(
    GEMINI_FIELD_DEFAULTS,
    VERTEXAI_UPSTREAM_METADATA
  );

  // Make sure we pass through even the body is not json
  setPath("/whatever?foo=bar"); // We don't preserve the qs param when re-writing the path
  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  EXPECT_EQ(std::string{"hello world"}, body_.toString());

  // valid json body with just empty object
  setBody("{}");
  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  auto parsed_body = json::parse(body_.toString());
  ASSERT_EQ(false, parsed_body.empty());
  EXPECT_EQ(std::string{"bar"}, parsed_body["foo"]) << "json: " << parsed_body.dump() << "\nstr: " << body_.toString();
  auto expected_json = json::parse(R"({"stopSequences": [ "foobar" ], "temperature": "0.2", "maxOutputTokens": 400})");
  EXPECT_EQ(expected_json, parsed_body["generationConfig"]) << "json: " << parsed_body.dump() << "\nstr: " << body_.toString();

  // Test override
  setBody(R"(
  {
    "contents": [
      {"role":"user", "parts":[{ "text": "Hello"}]}
    ],
    "generationConfig": {
      "temperature": "1.0",
      "maxOutputTokens": 800,
      "topP": "0.8",
      "topK": 10
    }
  }
  )");

  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  parsed_body = json::parse(body_.toString());
  // no model is set
  EXPECT_EQ(false, parsed_body.contains("model"));
  // We don't merge the json, so the entire generationConfig field is replaced
  EXPECT_EQ(expected_json, parsed_body["generationConfig"]) << "json: " << parsed_body.dump() << "\nstr: " << body_.toString();
  // contents is not changed
  expected_json = json::parse(R"([ {"role":"user", "parts":[{ "text": "Hello"}]} ])");
  EXPECT_EQ(expected_json, parsed_body["contents"]);
}

TEST_F(AiTransformerTest, VertexAIPromptEnrichmentTransformation) {
  auto aiTransformer = createAiTransformer(
    AI_PROMPT_ENRICHMENT,
    VERTEXAI_UPSTREAM_METADATA
  );

  setBody(R"(
  {
    "contents": [
      {"role":"user", "parts":[{ "text": "Hello!"}]}
    ]
  }
  )");

  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  auto parsed_body = json::parse(body_.toString());
  auto expected_system_instruction_json = json::parse(R"(
  [
    {"role": "system", "parts": [{"text": "you are a helpful assistant."}] },
    {"role": "developer", "parts": [{"text": "reply everything with programming analogy."}] },
    {"role": "developer", "parts": [{"text": "you are expert in assembly language."}] },
    {"role": "system", "parts": [{"text": "reply in British accent."}] }
  ]
  )");
  auto expected_contents_json = json::parse(R"(
  [
    {"role": "user", "parts": [{"text": "my name is Bond."}] },
    {"role": "user", "parts": [{"text": "Hello!"}] },
    {"role": "user", "parts": [{"text": "I live in the US."}] }
  ]
  )");
  EXPECT_EQ(expected_contents_json, parsed_body["contents"]) << "\nbody: " << body_.toString();
  EXPECT_EQ(expected_system_instruction_json, parsed_body["system_instruction"]) << "\nbody: " << body_.toString();
}

TEST_F(AiTransformerTest, BedrockPromptEnrichmentTransformation) {
  auto aiTransformer = createAiTransformer(
    AI_PROMPT_ENRICHMENT,
    BEDROCK_UPSTREAM_METADATA
  );

  setBody(R"(
  {
    "messages": [
      {"role":"user", "content":[{ "text": "Hello!"}]}
    ]
  }
  )");

  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  auto parsed_body = json::parse(body_.toString());
  auto expected_system_json = json::parse(R"(
  [
    {"text": "you are a helpful assistant."},
    {"text": "reply everything with programming analogy."},
    {"text": "you are expert in assembly language."},
    {"text": "reply in British accent."}
  ]
  )");
  auto expected_contents_json = json::parse(R"(
  [
    {"role": "user", "content": [{"text": "my name is Bond."}] },
    {"role": "user", "content": [{"text": "Hello!"}] },
    {"role": "user", "content": [{"text": "I live in the US."}] }
  ]
  )");
  EXPECT_EQ(expected_contents_json, parsed_body["messages"]) << "\nbody: " << body_.toString();
  EXPECT_EQ(expected_system_json, parsed_body["system"]) << "\nbody: " << body_.toString();

  // Test with existing system_instruction
  setBody(R"(
  {
    "system": [
      {"text": "You name is Bob."}
    ],
    "messages": [
      {"role":"user", "content":[{ "text": "Hello!"}]}
    ]
  }
  )");
  headers_.clear();
  aiTransformer->transform(headers_, &headers_, body_, filter_callbacks_);
  parsed_body = json::parse(body_.toString());
  expected_system_json = json::parse(R"(
  [
    {"text": "you are a helpful assistant."},
    {"text": "reply everything with programming analogy."},
    {"text": "You name is Bob."},
    {"text": "you are expert in assembly language."},
    {"text": "reply in British accent."}
  ]
  )");
  EXPECT_EQ(expected_contents_json, parsed_body["messages"]) << "\nbody: " << body_.toString();
  EXPECT_EQ(expected_system_json, parsed_body["system"]) << "\nbody: " << body_.toString();
}

} // namespace

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
