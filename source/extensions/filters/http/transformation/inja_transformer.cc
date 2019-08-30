#include "extensions/filters/http/transformation/inja_transformer.h"

#include <iterator>

#include "common/common/macros.h"

// For convenience
using namespace inja;
using json = nlohmann::json;

using namespace std::placeholders;

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {
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
    : headername_(extractor.header()), group_(extractor.subgroup()),
      extract_regex_(extractor.regex()) {}
std::string Extractor::extract(const Http::HeaderMap &header_map) const {
  // TODO: should we lowercase them in the config?
  const Http::HeaderEntry *header_entry = getHeader(header_map, headername_);
  if (!header_entry) {
    return "";
  }

  std::string value(header_entry->value().getStringView());

  // get and regex
  std::smatch regex_result;
  if (std::regex_match(value, regex_result, extract_regex_)) {
    std::smatch::iterator submatch_it = regex_result.begin();
    for (unsigned i = 0; i < group_; i++) {
      std::advance(submatch_it, 1);
      if (submatch_it == regex_result.end()) {
        return "";
      }
    }
    return *submatch_it;
  }

  return "";
}

TransformerInstance::TransformerInstance(
    const Http::HeaderMap &header_map,
    const std::unordered_map<std::string, std::string> &extractions,
    const json &context)
    : header_map_(header_map), extractions_(extractions), context_(context) {
  env_.add_callback("header", 1,
                    [this](Arguments args) { return header_callback(args); });
  env_.add_callback("extraction", 1, [this](Arguments args) {
    return extracted_callback(args);
  });
}

json TransformerInstance::header_callback(Arguments args) {
  std::string headername = args.at(0)->get<std::string>();
  const Http::HeaderEntry *header_entry =
      getHeader(header_map_, std::move(headername));
  if (!header_entry) {
    return "";
  }
  return std::string(header_entry->value().getStringView());
}

json TransformerInstance::extracted_callback(Arguments args) {
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

InjaTransformer::InjaTransformer(
    const envoy::api::v2::filter::http::TransformationTemplate &transformation)
    : advanced_templates_(transformation.advanced_templates()),
      passthrough_body_(transformation.has_passthrough()) {
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
    headers_.emplace_back(std::make_pair(std::move(header_name),
                                         parser.parse(it->second.text())));
  }

  switch (transformation.body_transformation_case()) {
  case envoy::api::v2::filter::http::TransformationTemplate::kBody: {
    body_template_.emplace(parser.parse(transformation.body().text()));
  }
  case envoy::api::v2::filter::http::TransformationTemplate::
      kMergeExtractorsToBody: {
    merged_extractors_to_body_ = true;
  }
  case envoy::api::v2::filter::http::TransformationTemplate::kPassthrough:
  case envoy::api::v2::filter::http::TransformationTemplate::
      BODY_TRANSFORMATION_NOT_SET: {
    break;
  }
  }
}

InjaTransformer::~InjaTransformer() {}

void InjaTransformer::transform(Http::HeaderMap &header_map,
                                Buffer::Instance &body) const {
  json json_body;
  if (body.length() > 0) {
    const std::string bodystring = body.toString();

    // parse the body as json
    json_body = json::parse(bodystring);
  }
  // get the extractions
  std::unordered_map<std::string, std::string> extractions;
  if (advanced_templates_) {
    extractions.reserve(extractors_.size());
  }

  for (const auto &named_extractor : extractors_) {
    const std::string &name = named_extractor.first;
    if (advanced_templates_) {
      extractions[name] = named_extractor.second.extract(header_map);
    } else {
      std::string name_to_split = name;
      json *current = &json_body;
      for (size_t pos = name_to_split.find("."); pos != std::string::npos;
           pos = name_to_split.find(".")) {
        auto &&field_name = name_to_split.substr(0, pos);
        current = &(*current)[field_name];
        name_to_split.erase(0, pos + 1);
      }
      (*current)[name_to_split] = named_extractor.second.extract(header_map);
    }
  }
  // start transforming!
  TransformerInstance instance(header_map, extractions, json_body);

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
}

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
