#pragma once

#include <regex>

#include "api/envoy/config/filter/http/transformation/v2/transformation_filter.pb.validate.h"

#include "envoy/http/filter.h"
#include "nlohmann/json.hpp"
#include "source/common/common/logger.h"
#include "source/common/common/matchers.h"
#include "source/common/common/utility.h"
#include "source/common/http/header_utility.h"
#include "source/common/regex/regex.h"
#include "source/common/singleton/const_singleton.h"
#include "transformer.h"


namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

struct AiTransformerValues {
  const std::string PROVIDER_ANTHROPIC {"anthropic"};
  const std::string PROVIDER_AZURE {"azure"};
  const std::string PROVIDER_GEMINI {"gemini"};
  const std::string PROVIDER_OPENAI {"openai"};
  const std::string PROVIDER_VERTEXAI {"vertexai"};

  const std::string SCHEMA_ANTHROPIC {"anthropic"};
  const std::string SCHEMA_GEMINI {"gemini"};
  const std::string SCHEMA_OPENAI {"openai"};

  const std::string GEMINI_GENERATE_CONTENT {"generateContent"};
  const std::string GEMINI_STREAM_GENERATE_CONTENT {"streamGenerateContent"};
  const std::string GEMINI_STREAM_QS_PARAM {"alt=sse"};

  const Http::LowerCaseString AnthropicApiKeyHeader{"x-api-key"};
  const Http::LowerCaseString AnthropicVersionHeader{"anthropic-version"};
  const Http::LowerCaseString AzureApiKeyHeader{"api-key"};
  const Http::LowerCaseString GeminiApiKeyHeader{"x-goog-api-key"};
};

struct PromptEnrichment {
  struct Message {
    Message(std::string role, std::string content) :
      role_(std::move(role)),
      content_(std::move(content)) {}
    virtual ~Message() = default;
    Message(const Message& other) = default; // Copy constructor
    Message& operator=(const Message& other) = default; // Copy assignment operator
    Message(Message&& other) = default; // Move constructor
    Message& operator=(Message&& other) = default; // Move assignment operator

    const std::string& role() const { return role_; }
    const std::string& content() const { return content_; }

  private:
    std::string role_;
    std::string content_;
  };

  PromptEnrichment(const envoy::api::v2::filter::http::PromptEnrichment &);
  virtual ~PromptEnrichment() = default;
  PromptEnrichment(const PromptEnrichment& other) = default; // Copy constructor
  PromptEnrichment& operator=(const PromptEnrichment& other) = default; // Copy assignment operator
  PromptEnrichment(PromptEnrichment&& other) = default; // Move constructor
  PromptEnrichment& operator=(PromptEnrichment&& other) = default; // Move assignment operator

  const std::vector<Message>& prepend() const { return prepend_; }

  const std::vector<Message>& append() const { return append_; }

private:
  std::vector<Message> prepend_;
  std::vector<Message> append_;

  template<typename... Args>
  void prependMessage(Args&&... args) { prepend_.emplace_back(std::forward<Args>(args)...); }

  template<typename... Args>
  void appendMessage(Args&&... args) { append_.emplace_back(std::forward<Args>(args)...); }
};

struct FieldDefault {
  FieldDefault(const envoy::api::v2::filter::http::FieldDefault &);
  virtual ~FieldDefault() = default;
  FieldDefault(const FieldDefault& other) = default; // Copy constructor
  FieldDefault& operator=(const FieldDefault& other) = default; // Copy assignment operator
  FieldDefault(FieldDefault&& other) = default; // Move constructor
  FieldDefault& operator=(FieldDefault&& other) = default; // Move assignment operator

  const std::string& field() const { return field_; }
  const nlohmann::json& value() const { return value_; }
  bool override() const { return override_; }

private:
  std::string field_;                  // field 1
  nlohmann::json value_;               // field 2
  bool override_ {false};              // field 3, defaults to false
};

using AiTransformerConstants = ConstSingleton<AiTransformerValues>;

class AiTransformer
    : public Envoy::Extensions::HttpFilters::Transformation::Transformer,
      public Logger::Loggable<Logger::Id::filter> {
public:
  AiTransformer(const envoy::api::v2::filter::http::AiTransformation &transformation,
                google::protobuf::BoolValue log_request_response_info);
  virtual ~AiTransformer() = default;

  void transform(Http::RequestOrResponseHeaderMap &map,
                 Http::RequestHeaderMap *request_map, Buffer::Instance &body,
                 Http::StreamFilterCallbacks &callbacks) const override;
  bool passthrough_body() const override { return false; };

private:
  std::tuple<bool, bool> transformHeaders(Http::RequestHeaderMap *request_map,
                                          Envoy::Upstream::MetadataConstSharedPtr endpoint_metadata,
                                          Http::StreamFilterCallbacks &callbacks,
                                          const std::string &model) const;
  void transformBody(Http::RequestHeaderMap *request_map,
                     Envoy::Upstream::MetadataConstSharedPtr endpoint_metadata,
                     Buffer::Instance &body,
                     Http::StreamFilterCallbacks &callbacks,
                     const std::string &model) const;

  bool enable_chat_streaming_ {false};
  std::vector<FieldDefault> field_defaults_;
  PromptEnrichment prompt_enrichment_;

};

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
