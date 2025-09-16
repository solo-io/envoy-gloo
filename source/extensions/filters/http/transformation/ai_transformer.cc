#include "ai_transformer.h"

#include <algorithm>
#include <functional>
#include <regex>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include "source/common/common/logger.h"
#include "source/common/common/regex.h"
#include "source/common/common/utility.h"
#include "source/common/config/metadata.h"
#include "source/common/http/header_utility.h"
#include "source/common/http/headers.h"
#include "source/common/protobuf/utility.h"
#include "source/extensions/filters/http/solo_well_known_names.h"

using json = nlohmann::json;

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

namespace {

/**
 * @brief Look up value from EndPointMetadata.
 *
 * @param endpoint_metadata
 * @param key can be ":" delimited to lookup nested value
 * @return std::string value pointed to by the key or empty string if key does
 * not exist
 */
std::string lookupEndpointMetadata(
    Envoy::Upstream::MetadataConstSharedPtr endpoint_metadata,
    const std::string &key) {
  static const char delimiter = ':';
  static const std::string trueString{"true"};
  static const std::string falseString{"false"};

  if (!endpoint_metadata) {
    return "";
  }

  std::vector<std::string> elements = absl::StrSplit(key, delimiter);
  const ProtobufWkt::Value &value = Envoy::Config::Metadata::metadataValue(
      endpoint_metadata.get(),
      Extensions::HttpFilters::SoloHttpFilterNames::get().Transformation,
      elements);

  switch (value.kind_case()) {
  case ProtobufWkt::Value::kStringValue: {
    return value.string_value();
  }
  case ProtobufWkt::Value::kNumberValue: {
    return std::to_string(value.number_value());
  }
  case ProtobufWkt::Value::kBoolValue: {
    const std::string &stringval =
        value.bool_value() ? trueString : falseString;
    return stringval;
  }
  case ProtobufWkt::Value::kStructValue: {
    std::string output;
    auto status =
        ProtobufUtil::MessageToJsonString(value.struct_value(), &output);
    return output;
  }
  case ProtobufWkt::Value::kListValue: {
    std::string output;
    auto status =
        ProtobufUtil::MessageToJsonString(value.list_value(), &output);
    return output;
  }
  case ProtobufWkt::Value::KIND_NOT_SET:
  case ProtobufWkt::Value::kNullValue:
    return "";
  }

  return "";
}

/**
 * @brief Replace the {{model}} path with the real model string
 *
 * @param original_path
 * @param model
 * @return std::string the path with model replaced
 */
std::string replaceModelInPath(std::string_view original_path,
                               std::string_view model) {
  return absl::StrReplaceAll(original_path, {{"{{model}}", model}});
}

/**
 * @brief lazily constructed singleton regex to match OpenAI Platform API
 *        endpoints. This same regex is also used in the AI extproc in
 *        solo-projects to bypass OpenAI Platform API from going to through
 *        the extproc, so if this regex is change, it should be changed in
 *        solo-projects/projects/ai-extension/ai_extension/ext_proc/server.py
 *        (search for open_ai_platform_api_regex)
 *
 * @return const Regex::CompiledGoogleReMatcher&
 */
const Regex::CompiledGoogleReMatcher &openAiPlatformApiRegex() {
  // This list of OpenAI Platform API endpoints can be found here:
  // https://platform.openai.com/docs/api-reference/ under the "Platform APIs" section
  // These are endpoints that we don't apply any AI specific features to, so we go into
  // "bypass mode" and we don't modify the URL or inject anything to the request/response
  // body.
  // Technically, the "realtime" API is not part of the Platform API but we are adding it
  // here for convenience as the behavior is the same
  // "responses" and "conversations" API are not part of the Platform API neither but
  // we will support that in passthrough mode as well.
  CONSTRUCT_ON_FIRST_USE(Regex::CompiledGoogleReMatcherNoSafetyChecks,
                         ".*(/v[0-9]+[a-z]*)(/"
                         "(audio|embeddings|fine_tuning|batches|files|uploads|"
                         "images|models|moderations|realtime|responses|conversations).*)");
}

/**
 * @brief Check if `path` is an OpenAI platform API endpoints. This includes
 * audio, embeddings, files ...etc See
 * https://platform.openai.com/docs/api-reference/ for details. If it's a
 * platform API request, this function will strip off any prefix in the path and
 * return the correspoinding API endpoints suitable to be sent to the OpenAI
 * server
 *
 * @param path is the original request path
 * @return std::tuple<bool, std::string> true and the rewritten API request if
 * it is an OpenAI platform API requests
 */
std::tuple<bool, std::string>
checkOpenAiPlatformApiRequest(std::string_view path) {
  auto is_platform_api = openAiPlatformApiRegex().match(path);
  if (is_platform_api) {
    return {true, openAiPlatformApiRegex().replaceAll(path, R"(\1\2)")};
  }

  // Not a Platform API request
  return {false, std::string{}};
}

/**
 * @brief Get the Request Path from the request header map
 *
 * @param request_headers
 * @return std::string_view empty if path does not exists
 */
std::string_view getRequestPath(Http::RequestHeaderMap *request_headers) {
  auto path = request_headers->Path();

  if (!path) {
    return "";
  }

  return path->value().getStringView();
}

/**
 * @brief Set the standard Bearer Auth Token Header in request map. If the
 * header already exists, it will be replaced
 *
 * @param request_headers
 * @param token
 */
void setBearerAuthTokenHeader(Http::RequestHeaderMap *request_headers,
                              absl::string_view token,
                              bool in_auth_token_passthru_mode) {
  if (token.empty() || in_auth_token_passthru_mode) {
    return;
  }

  request_headers->setReferenceKey(Http::CustomHeaders::get().Authorization,
                                   absl::StrCat("Bearer ", token));
}

/**
 * @brief Get the Token From standard Authorization Header object
 *
 * @param request_headers
 * @return std::string_view the token from the header with "Bearer " prefix
 * stripped off
 */
std::string_view
getTokenFromAuthorizationHeader(Http::RequestHeaderMap *request_headers) {
  auto result = request_headers->get(Http::CustomHeaders::get().Authorization);
  if (result.empty()) {
    return "";
  }

  auto value = result[0]->value().getStringView();
  if (value.size() >= 8 &&
      (value.starts_with("Bearer ") || value.starts_with("bearer "))) {
    return value.substr(7);
  }

  return value;
}

/**
 * @brief Helper function to check if a header already exists
 *
 * @param request_headers
 * @param key
 * @return true
 * @return false
 */
bool headerExists(Http::RequestHeaderMap *request_headers,
                  const Http::LowerCaseString &key) {
  if (request_headers->get(key).empty()) {
    return false;
  }

  return true;
}

/**
 * @brief Helper function to set the provider specific Key header. In auth token
 * pass thru mode, if the provider specific key header does not already exists,
 * we will extract the token from the Authorization header and put that into the
 * provider specific key header
 *
 * @param request_headers
 * @param key
 * @param auth_token
 * @param in_auth_token_passthru_mode
 */
void setProviderKeyHeader(Http::RequestHeaderMap *request_headers,
                          const Http::LowerCaseString &key,
                          std::string_view auth_token,
                          bool in_auth_token_passthru_mode) {
  if (auth_token.empty()) {
    return;
  }

  if (in_auth_token_passthru_mode) {
    // auth_token is extracted from the standard Authorization header and this
    // function is only used when the Provider is not using standard
    // Authorization header, so, if the Provider specific key header already
    // exists, let it pass through instead of using the token from Authorization
    // header.
    if (headerExists(request_headers, key)) {
      return;
    }
  }

  request_headers->setReferenceKey(key, auth_token);
}

/**
 * @brief Helper to convert protobuf Value to Json object. This function will be
 * called recursively to watch through any structure or list
 *
 * @param pb_value
 * @return json
 */
json protobufValueToJson(const ProtobufWkt::Value &pb_value) {
  switch (pb_value.kind_case()) {
  case ProtobufWkt::Value::kNullValue:
    return json(nullptr); // JSON null

  case ProtobufWkt::Value::kBoolValue:
    return json(pb_value.bool_value()); // JSON boolean

  case ProtobufWkt::Value::kNumberValue:
    return json(pb_value.number_value()); // JSON number (double)

  case ProtobufWkt::Value::kStringValue:
    return json(pb_value.string_value()); // JSON string

  case ProtobufWkt::Value::kStructValue: {
    json obj = json::object(); // JSON object
    const ProtobufWkt::Struct &pb_struct = pb_value.struct_value();
    for (const auto &field : pb_struct.fields()) {
      obj[field.first] = protobufValueToJson(field.second);
    }
    return obj;
  }

  case ProtobufWkt::Value::kListValue: {
    json arr = json::array(); // JSON array
    const ProtobufWkt::ListValue &pb_list = pb_value.list_value();
    for (const auto &item : pb_list.values()) {
      arr.push_back(protobufValueToJson(item));
    }
    return arr;
  }

  case ProtobufWkt::Value::KIND_NOT_SET:
  default:
    return json(nullptr); // Default to null if kind is not set
  }
}

enum class PromptAction { PREPEND, APPEND };
/**
 * @brief Helper function to add prompt for Gemini. The prompt can exist in
 * either the `contents` field for user prompt or the `system_instruction` field
 * for system prompt and the format is the same
 *
 * @param prompt
 * @param contents is the reference to the existing field, it could be the
 * `contents` field or the `system_instruction` field
 * @param action
 */
void addGeminiPrompts(const PromptEnrichment::Message &prompt,
                      json::array_t &contents, PromptAction action) {
  auto new_prompt = json::object();
  new_prompt["role"] = prompt.role();
  // note that creating the json::array like this:
  // json::array({{"text", prompt.content()}});
  // will sometimes create an array inside the array instead of object inside
  // the array So, need to be explicit that its an object inside the array
  new_prompt["parts"] =
      json::array({json::object({{"text", prompt.content()}})});

  if (action == PromptAction::PREPEND) {
    contents.insert(contents.begin(), std::move(new_prompt));
  } else {
    contents.push_back(std::move(new_prompt));
  }
}

/**
 * @brief Helper function to add prompts for Bedrock. The prompt can exist in
 * either the `messages` field for user prompt or the `system` field
 * for system prompt but the format are different. The `system` field is an array
 * but has no role for the object inside the array. The object can be a union of
 * other object type but we are only use "text" string here:
 * https://docs.aws.amazon.com/bedrock/latest/APIReference/API_runtime_SystemContentBlock.html
 *
 * If the `messages` field or the `system` field are missing, we will create it
 *
 * @param prompt
 * @param json_body
 * @param action
 * @return true if prompts are added successfully or false otherwise
 */
bool addBedrockPrompts(const PromptEnrichment::Message &prompt,
                      json &json_body, PromptAction action) {
  if (prompt.role() == "system" || prompt.role() == "developer") {
    if (!json_body.contains("system")) {
      json_body["system"] = json::array({});
    }
    auto &system_prompts = json_body["system"];
    if (!system_prompts.is_array()) {
      return false;
    }

    auto new_prompt = json::object({{"text", prompt.content()}});
    if (action == PromptAction::PREPEND) {
      system_prompts.insert(system_prompts.begin(), std::move(new_prompt));
    } else {
      system_prompts.push_back(std::move(new_prompt));
    }

    return true;
  }

  // Assume all other roles as user prompts
  if (!json_body.contains("messages")) {
    json_body["messages"] = json::array({});
  }
  auto &messages = json_body["messages"];
  if (!messages.is_array()) {
    return false;
  }
  auto new_prompt = json::object();
  new_prompt["role"] = prompt.role();
  new_prompt["content"] =
      json::array({json::object({{"text", prompt.content()}})});

  if (action == PromptAction::PREPEND) {
    messages.insert(messages.begin(), std::move(new_prompt));
  } else {
    messages.push_back(std::move(new_prompt));
  }
  return true;
}

/**
 * @brief prepend or append prompts into the existing prompts in `json_body`
 *
 * @param schema the api schema used in `json_body`
 * @param json_body the json_body that contains the original prompts, this will
 * be modified in place
 * @param prompts contains system or user prompt
 * @param action enum to determine if it is prepend or append
 *
 * @return true if successful
 * @return false if the json_body does not contain the correct object containing
 * the existing prompt
 */
bool addPrompts(const std::string &schema, json &json_body,
                const PromptEnrichment::Message &prompt, PromptAction action) {
  if (schema == AiTransformerConstants::get().SCHEMA_GEMINI) {
    if (prompt.role() == "system" || prompt.role() == "developer") {
      if (!json_body.contains("system_instruction")) {
        json_body["system_instruction"] = json::array({});
      }
      auto &value = json_body["system_instruction"];
      if (!value.is_array()) {
        return false;
      }
      auto &contents = value.get_ref<json::array_t &>();
      addGeminiPrompts(prompt, contents, action);
    } else {
      if (!json_body.contains("contents")) {
        return false;
      }
      auto &value = json_body["contents"];
      if (!value.is_array()) {
        return false;
      }
      auto &contents = value.get_ref<json::array_t &>();
      addGeminiPrompts(prompt, contents, action);
    }
  } else if (schema == AiTransformerConstants::get().SCHEMA_BEDROCK) {
    return addBedrockPrompts(prompt, json_body, action);
  } else {
    if (!json_body.contains("messages")) {
      return false;
    }
    auto &messages = json_body["messages"];
    if (!messages.is_array()) {
      return false;
    }

    // OpenAI
    auto new_prompt = json::object();
    new_prompt["role"] = prompt.role();
    new_prompt["content"] = prompt.content();

    if (action == PromptAction::PREPEND) {
      messages.insert(messages.begin(), std::move(new_prompt));
    } else {
      messages.push_back(std::move(new_prompt));
    }
  }

  return true;
}

/**
 * @brief Helper function to add prompts in the correct field for various api
 * schema
 *
 * @param schema the api schema used in `json_body`
 * @param json_body the json_body that contains the original prompts, this will
 * be modified in place
 * @param prepend_prompts list of prompts to prepend to existing prompts
 * @param append_prompts list of prompts to append to existing prompts
 * @return true if prompts are successfully added
 * @return false if there is no existing prompt field
 */
bool addPrompts(const std::string &schema, json &json_body,
                const std::vector<PromptEnrichment::Message> &prepend_prompts,
                const std::vector<PromptEnrichment::Message> &append_prompts) {
  std::string anthropic_system_prompt;
  std::string anthropic_developer_prompt;
  std::vector<std::reference_wrapper<const PromptEnrichment::Message>>
      reversed_prepend_prompts;
  reversed_prepend_prompts.reserve(prepend_prompts.size());
  for (auto &prompt : prepend_prompts) {
    if (schema == AiTransformerConstants::get().SCHEMA_ANTHROPIC) {
      // Anthropic system/developer prompts go to a seperate `system` filed as a
      // single string. So, accumulate them separately and then set them in the
      // `system` at the end Each prompt is separated by a newline when
      // combining into a single string

      if (prompt.role() == "system") {
        absl::StrAppend(&anthropic_system_prompt, prompt.content(), "\n");
        continue;
      } else if (prompt.role() == "developer") {
        absl::StrAppend(&anthropic_developer_prompt, prompt.content(), "\n");
        continue;
      }
    }

    reversed_prepend_prompts.insert(reversed_prepend_prompts.begin(), prompt);
  }

  // Have to create a vector to store the prompt in reverse order because some
  // provider uses separate field for system prompt, so cannot just track the
  // order with a single offset index.
  for (auto &prompt : reversed_prepend_prompts) {
    if (!addPrompts(schema, json_body, prompt, PromptAction::PREPEND)) {
      return false;
    }
  }

  for (auto &prompt : append_prompts) {
    if (schema == AiTransformerConstants::get().SCHEMA_ANTHROPIC) {
      // Anthropic system/developer prompts go to a seperate `system` filed as a
      // single string. So, accumulate them separately and then set them in the
      // `system` at the end Each prompt is separated by a newline when
      // combining into a single string

      if (prompt.role() == "system") {
        absl::StrAppend(&anthropic_system_prompt, prompt.content(), "\n");
        continue;
      } else if (prompt.role() == "developer") {
        absl::StrAppend(&anthropic_developer_prompt, prompt.content(), "\n");
        continue;
      }
    }

    if (!addPrompts(schema, json_body, prompt, PromptAction::APPEND)) {
      return false;
    }
  }

  if (!anthropic_system_prompt.empty() || !anthropic_developer_prompt.empty()) {
    const std::string *existing_system_prompt = nullptr;
    if (json_body.contains("system") && json_body["system"].is_string()) {
      existing_system_prompt =
          json_body["system"].get_ptr<const json::string_t *>();
    }
    if (!existing_system_prompt) {
      json_body["system"] = absl::StrCat(anthropic_system_prompt, "\n",
                                         anthropic_developer_prompt);
    } else {
      // For Anthropic, we group system and developer prompt separately and
      // always append to existing system prompt regardless if they are in the
      // prepend or append prompts as I don't think the prepend/append concept
      // applies to Anthropic's single system prompt field.
      json_body["system"] =
          absl::StrCat(*existing_system_prompt, "\n", anthropic_system_prompt,
                       "\n", anthropic_developer_prompt);
    }
  }

  return true;
}

} // namespace

PromptEnrichment::PromptEnrichment(
    const envoy::api::v2::filter::http::PromptEnrichment &pe) {
  for (const auto &prompt : pe.append()) {
    appendMessage(prompt.role(), prompt.content());
  }

  for (const auto &prompt : pe.prepend()) {
    prependMessage(prompt.role(), prompt.content());
  }
}

FieldDefault::FieldDefault(
    const envoy::api::v2::filter::http::FieldDefault &field_default)
    : field_(field_default.field()),
      value_(protobufValueToJson(field_default.value())),
      override_(field_default.override()) {}

AiTransformer::AiTransformer(
    const envoy::api::v2::filter::http::AiTransformation &transformation,
    google::protobuf::BoolValue log_request_response_info)
    : Transformer(log_request_response_info),
      enable_chat_streaming_(transformation.enable_chat_streaming()),
      prompt_enrichment_(transformation.prompt_enrichment()) {
  for (const auto &field_default : transformation.field_defaults()) {
    field_defaults_.emplace_back(field_default);
  }
};

std::tuple<bool, bool> AiTransformer::transformHeaders(
    Http::RequestHeaderMap *request_headers,
    Envoy::Upstream::MetadataConstSharedPtr endpoint_metadata,
    Http::StreamFilterCallbacks &callbacks, const std::string &model) const {
  std::string path;
  bool in_bypass_mode = false;
  bool update_model_in_body = false;
  auto provider = lookupEndpointMetadata(endpoint_metadata, "provider");
  bool in_auth_token_passthru_mode = false;
  auto auth_token = lookupEndpointMetadata(endpoint_metadata, "auth_token");
  if (auth_token.empty()) {
    in_auth_token_passthru_mode = true;
    auth_token = getTokenFromAuthorizationHeader(request_headers);
  }

  std::string_view original_path = getRequestPath(request_headers);
  if (provider == AiTransformerConstants::get().PROVIDER_AZURE) {
    ASSERT(!model.empty(), "Azure OpenAI: required model setting is missing!");
    path = replaceModelInPath(lookupEndpointMetadata(endpoint_metadata, "path"),
                              model);
    setProviderKeyHeader(request_headers,
                         AiTransformerConstants::get().AzureApiKeyHeader,
                         auth_token, in_auth_token_passthru_mode);

  } else if (provider == AiTransformerConstants::get().PROVIDER_GEMINI) {
    path = lookupEndpointMetadata(endpoint_metadata, "path");
    if (path.empty()) {
      ASSERT(!model.empty(), "Gemini: required model setting is missing!");
      path = replaceModelInPath(
          lookupEndpointMetadata(endpoint_metadata, "base_path"), model);
      if (enable_chat_streaming_) {
        absl::StrAppend(
            &path, AiTransformerConstants::get().GEMINI_STREAM_GENERATE_CONTENT,
            "?", AiTransformerConstants::get().GEMINI_STREAM_QS_PARAM);
      } else {
        absl::StrAppend(&path,
                        AiTransformerConstants::get().GEMINI_GENERATE_CONTENT);
      }
    }
    // Gemini doc is still using the `key` qs param but the Google GenAI sdk has
    // switched to use the `x-goog-api-key` header. Here is the reason we also
    // switch to the header:
    //       1) currently the auth passthrough only work because the gemini sdk
    //       is using this header.
    //          If someone is using the `key` qs param, passthrough never works.
    //          (Should we support that use case?)
    //       2) putting the key in the qs param will probably get logged in
    //       access log which is a security concern
    setProviderKeyHeader(request_headers,
                         AiTransformerConstants::get().GeminiApiKeyHeader,
                         auth_token, in_auth_token_passthru_mode);

  } else if (provider == AiTransformerConstants::get().PROVIDER_BEDROCK) {
    path = lookupEndpointMetadata(endpoint_metadata, "path");
    if (path.empty()) {
      ASSERT(!model.empty(), "Bedrock: required model setting is missing!");
      path = replaceModelInPath(
          lookupEndpointMetadata(endpoint_metadata, "base_path"), model);
      if (enable_chat_streaming_) {
        absl::StrAppend(&path, AiTransformerConstants::get().BEDROCK_CONVERSE_STREAM);
      } else {
        absl::StrAppend(&path, AiTransformerConstants::get().BEDROCK_CONVERSE);
      }
    }
  } else if (provider == AiTransformerConstants::get().PROVIDER_VERTEXAI) {
    path = lookupEndpointMetadata(endpoint_metadata, "path");
    if (path.empty()) {
      ASSERT(!model.empty(), "VertexAI: required model setting is missing!");
      path = replaceModelInPath(
          lookupEndpointMetadata(endpoint_metadata, "base_path"), model);
      auto model_path = lookupEndpointMetadata(endpoint_metadata, "model_path");
      if (model_path.empty()) {
        if (enable_chat_streaming_) {
          absl::StrAppend(
              &path, AiTransformerConstants::get().GEMINI_STREAM_GENERATE_CONTENT,
              "?", AiTransformerConstants::get().GEMINI_STREAM_QS_PARAM);
        } else {
          absl::StrAppend(&path,
                          AiTransformerConstants::get().GEMINI_GENERATE_CONTENT);
        }
      } else {
        // Assuming model_path contains the correct qs params as well
        absl::StrAppend(&path, model_path);
      }
    }

    setBearerAuthTokenHeader(request_headers, auth_token,
                             in_auth_token_passthru_mode);
  } else {
    // Other providers that uses OpenAI API
    auto [is_platform_api_request, new_path] =
        checkOpenAiPlatformApiRequest(original_path);
    ENVOY_STREAM_LOG(debug, "path from regex: {}", callbacks, new_path);

    if (is_platform_api_request) {
      // platform_api_base_path is set by the control plane when there is a
      // Custom PathOverride with BasePath
      auto base_path = lookupEndpointMetadata(endpoint_metadata, "platform_api_base_path");
      if (base_path.empty()) {
        path = std::move(new_path);
      } else {
        path = absl::StrCat(base_path, new_path);
      }
      in_bypass_mode = true;
    } else {
      if (!model.empty()) {
        update_model_in_body = true;
      }
      path = lookupEndpointMetadata(endpoint_metadata, "path");
      if (path.empty()) {
        path = "/v1/chat/completions";
      }
    }

    if (provider == AiTransformerConstants::get().PROVIDER_ANTHROPIC) {
      auto version = lookupEndpointMetadata(endpoint_metadata, "version");
      if (!version.empty()) {
        request_headers->setReferenceKey(
            AiTransformerConstants::get().AnthropicVersionHeader, version);
      }
      setProviderKeyHeader(request_headers,
                           AiTransformerConstants::get().AnthropicApiKeyHeader,
                           auth_token, in_auth_token_passthru_mode);
    } else {
      setBearerAuthTokenHeader(request_headers, auth_token,
                               in_auth_token_passthru_mode);
    }
  }

  if (!path.empty()) {
    ENVOY_STREAM_LOG(debug, "changing path from {} to {}", callbacks,
                     original_path, path);
    request_headers->setPath(path);
  }

  return {in_bypass_mode, update_model_in_body};
}

void AiTransformer::transformBody(
    Http::RequestHeaderMap *request_headers,
    Envoy::Upstream::MetadataConstSharedPtr endpoint_metadata,
    Buffer::Instance &body, Http::StreamFilterCallbacks &callbacks,
    const std::string &model) const {
  bool body_modified = false;
  json json_body;
  auto parseJson = [&json_body, &body, &callbacks]() -> bool {
    if (!json_body.empty()) {
      return true;
    }
    try {
      json_body = json::parse(body.toString());
    } catch (const std::exception &) {
      ENVOY_STREAM_LOG(warn, "Failed to parse body as json. Passing through.",
                       callbacks);
      return false;
    }

    return true;
  };

  if (!model.empty()) {
    if (!parseJson()) {
      return;
    }
    if (!json_body.contains("model") || json_body["model"] != model) {
      json_body["model"] = model;
      body_modified = true;
    }
  }

  if (field_defaults_.size() > 0) {
    if (!parseJson()) {
      return;
    }

    for (auto &field : field_defaults_) {
      if (!field.override() && json_body.contains(field.field())) {
        // it's not overriding and field already exists
        continue;
      }

      json_body[field.field()] = field.value();
      body_modified = true;
    }
  }

  auto json_schema = lookupEndpointMetadata(endpoint_metadata, "json_schema");
  auto &prepend_prompts = prompt_enrichment_.prepend();
  auto &append_prompts = prompt_enrichment_.append();
  if (prepend_prompts.size() > 0 || append_prompts.size() > 0) {
    if (!parseJson()) {
      return;
    }

    if (!addPrompts(json_schema, json_body, prepend_prompts, append_prompts)) {
      ENVOY_STREAM_LOG(error, "failed to add prompts!", callbacks);
    } else {
      body_modified = true;
    }
  }

  if (enable_chat_streaming_) {
    if (json_schema == AiTransformerConstants::get().SCHEMA_OPENAI ||
        json_schema == AiTransformerConstants::get().SCHEMA_ANTHROPIC) {
      json_body["stream"] = bool(true);
      body_modified = true;
    }
    if (json_schema == AiTransformerConstants::get().SCHEMA_OPENAI) {
      if (!json_body.contains("stream_options")) {
        json_body["stream_options"] = json::object();
      }
      json_body["stream_options"]["include_usage"] = true;
      body_modified = true;
    }
  }

  if (body_modified) {
    request_headers->removeContentLength();
    body.drain(body.length());
    body.add(json_body.dump());
    request_headers->setContentLength(body.length());
  }
}

void AiTransformer::transform(Http::RequestOrResponseHeaderMap & /*header_map*/,
                              Http::RequestHeaderMap *request_headers,
                              Buffer::Instance &body,
                              Http::StreamFilterCallbacks &callbacks) const {

  if (!request_headers) {
    ENVOY_STREAM_LOG(warn, "request_headers is null!", callbacks);
    return;
  }

  // If there is a value we're in a upstream filter
  if (!callbacks.upstreamCallbacks().has_value()) {
    return;
  }
  auto &upstream_callbacks = callbacks.upstreamCallbacks().value().get();
  // Double check that upstream_host exists as if the wait_filter isn't properly
  // setup this can segfault
  auto upstreamHost =
      upstream_callbacks.upstreamStreamInfo().upstreamInfo()->upstreamHost();
  if (!upstreamHost) {
    ENVOY_STREAM_LOG(warn, "upstreamHost is null!", callbacks);
    return;
  }

  Envoy::Upstream::MetadataConstSharedPtr endpoint_metadata =
      upstreamHost->metadata();
  if (!endpoint_metadata) {
    ENVOY_STREAM_LOG(warn, "endpoint_metadata is null.", callbacks)
    return;
  }

  auto model = lookupEndpointMetadata(endpoint_metadata, "model");
  auto [in_bypass_mode, update_model_in_body] =
      transformHeaders(request_headers, endpoint_metadata, callbacks, model);
  if (in_bypass_mode || body.length() == 0) {
    return;
  }

  transformBody(request_headers, endpoint_metadata, body, callbacks,
                update_model_in_body ? model : std::string{""});
}

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
