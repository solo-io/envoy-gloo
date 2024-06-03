#pragma once

#include <map>

#include "envoy/buffer/buffer.h"
#include "envoy/http/header_map.h"
#include "envoy/common/random_generator.h"

#include "source/common/common/base64.h"

#include "envoy/thread_local/thread_local_object.h"
#include "envoy/thread_local/thread_local.h"
#include "source/extensions/filters/http/transformation/transformer.h"

// clang-format off
#include "nlohmann/json.hpp"
#include "inja/inja.hpp"
// clang-format on

#include "api/envoy/config/filter/http/transformation/v2/transformation_filter.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

using GetBodyFunc = std::function<const std::string &()>;
using ExtractionApi = envoy::api::v2::filter::http::Extraction;

struct ThreadLocalTransformerContext : public ThreadLocal::ThreadLocalObject {
public:
  ThreadLocalTransformerContext(){}

  const Http::RequestOrResponseHeaderMap *header_map_;
  const Http::RequestHeaderMap *request_headers_;
  const GetBodyFunc *body_;
  const std::unordered_map<std::string, std::string> *destructive_extractions_;
  const std::unordered_map<std::string, absl::string_view> *extractions_;
  const nlohmann::json *context_;
  const std::unordered_map<std::string, std::string> *environ_;
  const envoy::config::core::v3::Metadata *cluster_metadata_;
  Envoy::Upstream::MetadataConstSharedPtr endpoint_metadata_;
  const envoy::config::core::v3::Metadata *dynamic_metadata_;
};


class TransformerInstance {
public:
  TransformerInstance(ThreadLocal::Slot& tls, Envoy::Random::RandomGenerator &rng);

  inja::Template parse(std::string_view input);
  std::string render(const inja::Template &input);
  void set_element_notation(inja::ElementNotation notation) {
      env_.set_element_notation(notation);
  };
  // Sets the config for rendering strings raw or unescaped
  void set_escape_strings(bool escape_strings) {
      env_.set_escape_strings(escape_strings);
  };

private:
  // header_value(name)
  nlohmann::json header_callback(const inja::Arguments &args) const;
  nlohmann::json request_header_callback(const inja::Arguments &args) const;
  // extracted_value(name, index)
  nlohmann::json extracted_callback(const inja::Arguments &args) const;
  nlohmann::json dynamic_metadata(const inja::Arguments &args) const;
  nlohmann::json env(const inja::Arguments &args) const;
  nlohmann::json cluster_metadata_callback(const inja::Arguments &args) const;
  nlohmann::json cluster_metadata_callback_deprecated(const inja::Arguments &args) const;
  nlohmann::json dynamic_metadata_callback(const inja::Arguments &args) const;
  nlohmann::json host_metadata_callback(const inja::Arguments &args) const;
  nlohmann::json base64_encode_callback(const inja::Arguments &args) const;
  nlohmann::json base64url_encode_callback(const inja::Arguments &args) const;
  nlohmann::json base64_decode_callback(const inja::Arguments &args) const;
  nlohmann::json base64url_decode_callback(const inja::Arguments &args) const;
  nlohmann::json substring_callback(const inja::Arguments &args) const;
  nlohmann::json replace_with_random_callback(const inja::Arguments &args);
  std::string& random_for_pattern(const std::string& pattern);
  nlohmann::json raw_string_callback(const inja::Arguments &args) const;
  static nlohmann::json parse_metadata(const envoy::config::core::v3::Metadata* metadata,
                                                  const inja::Arguments &args);
  static nlohmann::json word_count_callback(const inja::Arguments &args);
  static int json_word_count(const nlohmann::json* str);
  static int word_count(const std::string& str);

  inja::Environment env_;
  absl::flat_hash_map<std::string, std::string> pattern_replacements_;
  ThreadLocal::Slot &tls_;
  Envoy::Random::RandomGenerator &rng_;
};

class Extractor : Logger::Loggable<Logger::Id::filter> {
public:
  Extractor(const envoy::api::v2::filter::http::Extraction &extractor);
  absl::string_view extract(Http::StreamFilterCallbacks &callbacks,
                            const Http::RequestOrResponseHeaderMap &header_map,
                            GetBodyFunc &body) const;
  std::string extractDestructive(Http::StreamFilterCallbacks &callbacks,
                      const Http::RequestOrResponseHeaderMap &header_map,
                      GetBodyFunc &body) const;
  const ExtractionApi::Mode& mode() const { return mode_; }
private:
  absl::string_view extractValue(Http::StreamFilterCallbacks &callbacks,
                                 absl::string_view value) const;
  std::string replaceIndividualValue(Http::StreamFilterCallbacks &callbacks,
                                           absl::string_view value) const;
  std::string replaceAllValues(Http::StreamFilterCallbacks &callbacks,
                                     absl::string_view value) const;

  const Http::LowerCaseString headername_;
  const bool body_;
  const unsigned int group_;
  const std::regex extract_regex_;
  const std::optional<const std::string> replacement_text_;
  const ExtractionApi::Mode mode_;
};

class InjaTransformer : public Transformer {
public:
  InjaTransformer(const envoy::api::v2::filter::http::TransformationTemplate &transformation,
                  Envoy::Random::RandomGenerator &rng,
                  google::protobuf::BoolValue log_request_response_info,
                  ThreadLocal::SlotAllocator &tls_);
  ~InjaTransformer();

  void transform(Http::RequestOrResponseHeaderMap &map,
                 Http::RequestHeaderMap *request_headers,
                 Buffer::Instance &body,
                 Http::StreamFilterCallbacks &) const override;
  bool passthrough_body() const override { return passthrough_body_; };

private:
  struct DynamicMetadataValue {
    std::string namespace_;
    std::string key_;
    inja::Template template_;
    bool parse_json_;
  };

  bool advanced_templates_{};
  bool passthrough_body_{};
  std::vector<std::pair<std::string, Extractor>> extractors_;
  std::vector<std::pair<Http::LowerCaseString, inja::Template>> headers_;
  std::vector<std::pair<Http::LowerCaseString, inja::Template>> headers_to_append_;
  std::vector<Http::LowerCaseString> headers_to_remove_;
  std::vector<DynamicMetadataValue> dynamic_metadata_;
  std::unordered_map<std::string, std::string> environ_;

  envoy::api::v2::filter::http::TransformationTemplate::RequestBodyParse
      parse_body_behavior_;
  bool ignore_error_on_parse_;
  bool escape_characters_{};

  absl::optional<inja::Template> body_template_;
  bool merged_extractors_to_body_{};
  ThreadLocal::SlotPtr tls_;
  std::unique_ptr<TransformerInstance> instance_;
};

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
