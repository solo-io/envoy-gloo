#include "extensions/filters/http/solo_well_known_names.h"
#include "extensions/filters/http/transformation/inja_transformer.h"

#include <iterator>

#include "common/common/macros.h"
#include "common/common/utility.h"
#include "common/common/regex.h"

// For convenience
using namespace inja;
using json = nlohmann::json;

using namespace std::placeholders;

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

using TransformationTemplate =
    envoy::api::v2::filter::http::TransformationTemplate;

// TODO: move to common
namespace {
const Http::HeaderEntry *getHeader(const Http::HeaderMap &header_map,
                                   const Http::LowerCaseString &key) {
  const Http::HeaderEntry *header_entry = header_map.get(key);
  if (!header_entry) {
    header_map.lookup(key, &header_entry);
  }
  return header_entry;
}

const Http::HeaderEntry *getHeader(const Http::HeaderMap &header_map,
                                   const std::string &key) {
  // use explicit consturctor so string is lowered
  auto lowerkey = Http::LowerCaseString(key);
  return getHeader(header_map, lowerkey);
}

} // namespace

Extractor::Extractor(const envoy::api::v2::filter::http::Extraction &extractor)
    : headername_(extractor.header()), body_(extractor.has_body()),
      group_(extractor.subgroup()),
      extract_regex_(Regex::Utility::parseStdRegex(extractor.regex())) {}

absl::string_view Extractor::extract(const Http::HeaderMap &header_map,
                                     GetBodyFunc body) const {
  // TODO: should we lowercase them in the config?
  if (body_) {
    return extractValue(body());
  } else {
    const Http::HeaderEntry *header_entry = getHeader(header_map, headername_);
    if (!header_entry) {
      return "";
    }
    return extractValue(header_entry->value().getStringView());
  }
}

absl::string_view Extractor::extractValue(absl::string_view value) const {
  // get and regex
  std::match_results<absl::string_view::const_iterator> regex_result;
  if (std::regex_match(value.begin(), value.end(), regex_result,
                       extract_regex_)) {
    if (group_ >= regex_result.size()) {
      // TODO: this indicates user misconfiguration... log or stat?
      return "";
    }
    const auto &sub_match = regex_result[group_];
    return absl::string_view(sub_match.first, sub_match.length());
  }
  return "";
}

TransformerInstance::TransformerInstance(
    const Http::HeaderMap &header_map, GetBodyFunc body,
    const std::unordered_map<std::string, absl::string_view> &extractions,
    const json &context)
    : header_map_(header_map), body_(body), extractions_(extractions),
      context_(context) {
  env_.add_callback("header", 1,
                    [this](Arguments& args) { return header_callback(args); });
  env_.add_callback("extraction", 1, [this](Arguments& args) {
    return extracted_callback(args);
  });
  env_.add_callback("body", 0, [this](Arguments&) { return body_(); });
}

json TransformerInstance::header_callback(const inja::Arguments& args) {
  std::string headername = args.at(0)->get<std::string>();
  const Http::HeaderEntry *header_entry = getHeader(header_map_, headername);
  if (!header_entry) {
    return "";
  }
  return std::string(header_entry->value().getStringView());
}

json TransformerInstance::extracted_callback(const inja::Arguments& args) {
  std::string name = args.at(0)->get<std::string>();
  const auto value_it = extractions_.find(name);
  if (value_it != extractions_.end()) {
    return value_it->second;
  }
  return "";
}

std::string TransformerInstance::render(const inja::Template &input) {
  return env_.render(input, context_);
}

InjaTransformer::InjaTransformer(const TransformationTemplate &transformation)
    : advanced_templates_(transformation.advanced_templates()),
      passthrough_body_(transformation.has_passthrough()),
      parse_body_behavior_(transformation.parse_body_behavior()),
      ignore_error_on_parse_(transformation.ignore_error_on_parse()) {
  inja::ParserConfig parser_config;
  inja::LexerConfig lexer_config;
  inja::TemplateStorage template_storage;
  if (!advanced_templates_) {
    parser_config.notation = inja::ElementNotation::Dot;
  }

  inja::Parser parser(parser_config, lexer_config, template_storage);

  const auto &extractors = transformation.extractors();
  for (auto it = extractors.begin(); it != extractors.end(); it++) {
    extractors_.emplace_back(std::make_pair(it->first, it->second));
  }
  const auto &headers = transformation.headers();
  for (auto it = headers.begin(); it != headers.end(); it++) {
    Http::LowerCaseString header_name(it->first);
    try {
      headers_.emplace_back(std::make_pair(std::move(header_name),
                                           parser.parse(it->second.text())));
    } catch (const std::runtime_error &e) {
      throw EnvoyException(fmt::format(
          "Failed to parse header template '{}': {}", it->first, e.what()));
    }
  }
  const auto &dynamic_metadata_values = transformation.dynamic_metadata_values();
  for (auto it = dynamic_metadata_values.begin(); it != dynamic_metadata_values.end(); it++) {
    try {
      DynamicMetadataValue dynamicMetadataValue;
      dynamicMetadataValue.namespace_ = it->metadata_namespace();
      if (dynamicMetadataValue.namespace_.empty()){
        dynamicMetadataValue.namespace_ = SoloHttpFilterNames::get().Transformation;
      }
      dynamicMetadataValue.key_ = it->key();
      dynamicMetadataValue.template_ = parser.parse(it->value().text());
      dynamic_metadata_.emplace_back(std::move(dynamicMetadataValue));
    } catch (const std::runtime_error &e) {
      throw EnvoyException(fmt::format(
          "Failed to parse header template '{}': {}", it->key(), e.what()));
    }
  }

  switch (transformation.body_transformation_case()) {
  case TransformationTemplate::kBody: {
    try {
      body_template_.emplace(parser.parse(transformation.body().text()));
    } catch (const std::runtime_error &e) {
      throw EnvoyException(
          fmt::format("Failed to parse body template {}", e.what()));
    }
    break;
  }
  case TransformationTemplate::kMergeExtractorsToBody: {
    merged_extractors_to_body_ = true;
    break;
  }
  case TransformationTemplate::kPassthrough:
    break;
  case TransformationTemplate::BODY_TRANSFORMATION_NOT_SET: {
    break;
  }
  }
}

InjaTransformer::~InjaTransformer() {}

void InjaTransformer::transform(Http::HeaderMap &header_map,
                                Buffer::Instance &body,
                                Http::StreamFilterCallbacks &callbacks) const {
  absl::optional<std::string> string_body;
  auto get_body = [&string_body, &body]() -> const std::string & {
    if (!string_body.has_value()) {
      string_body.emplace(body.toString());
    }
    return string_body.value();
  };

  json json_body;

  if (parse_body_behavior_ != TransformationTemplate::DontParse &&
      body.length() > 0) {
    const std::string &bodystring = get_body();
    // parse the body as json
    // TODO: gate this under a parse_body boolean
    if (parse_body_behavior_ == TransformationTemplate::ParseAsJson) {
      if (ignore_error_on_parse_) {
        try {
          json_body = json::parse(bodystring);
        } catch (std::exception &) {
        }
      } else {
        json_body = json::parse(bodystring);
      }
    } else {
      ASSERT("missing behavior");
    }
  }
  // get the extractions
  std::unordered_map<std::string, absl::string_view> extractions;
  if (advanced_templates_) {
    extractions.reserve(extractors_.size());
  }

  for (const auto &named_extractor : extractors_) {
    const std::string &name = named_extractor.first;
    if (advanced_templates_) {
      extractions[name] = named_extractor.second.extract(header_map, get_body);
    } else {
      absl::string_view name_to_split = name;
      json *current = &json_body;
      for (size_t pos = name_to_split.find("."); pos != std::string::npos;
           pos = name_to_split.find(".")) {
        auto &&field_name = name_to_split.substr(0, pos);
        current = &(*current)[std::string(field_name)];
        name_to_split = name_to_split.substr(pos + 1);
      }
      (*current)[std::string(name_to_split)] =
          named_extractor.second.extract(header_map, get_body);
    }
  }
  // start transforming!
  TransformerInstance instance(header_map, get_body, extractions, json_body);

  // Body transform:
  auto replace_body = [&](std::string &output) {
    // remove content length, as we have new body.
    header_map.removeContentLength();
    // replace body
    body.drain(body.length());
    body.add(output);
    header_map.insertContentLength().value(body.length());
  };

  if (body_template_.has_value()) {
    std::string output = instance.render(body_template_.value());
    replace_body(output);
  } else if (merged_extractors_to_body_) {
    std::string output = json_body.dump();
    replace_body(output);
  }

  // Headers transform:
  for (const auto &templated_header : headers_) {
    std::string output = instance.render(templated_header.second);
    // remove existing header
    header_map.remove(templated_header.first);
    // TODO(yuval-k): Do we need to support intentional empty headers?
    if (!output.empty()) {
      // we can add the key as reference as the headers_ lifetime is as the
      // route's
      header_map.addReferenceKey(templated_header.first, output);
    }
  }
  // DynamicMetadata transform:
  for (const auto &templated_dynamic_metadata : dynamic_metadata_) {
    std::string output = instance.render(templated_dynamic_metadata.template_);
    ProtobufWkt::Struct strct(MessageUtil::keyValueStruct(templated_dynamic_metadata.key_, output));
    callbacks.streamInfo().setDynamicMetadata(templated_dynamic_metadata.namespace_, strct);
  }
}

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
