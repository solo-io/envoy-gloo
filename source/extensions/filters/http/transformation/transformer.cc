#include "extensions/filters/http/transformation/transformer.h"

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

std::string ExtractorUtil::extract(
    const envoy::api::v2::filter::http::Extraction &extractor,
    const Http::HeaderMap &header_map) {
  // TODO: should we lowercase them in the config?
  const std::string &headername = extractor.header();
  const Http::HeaderEntry *header_entry = getHeader(header_map, headername);
  if (!header_entry) {
    return "";
  }

  std::string value(header_entry->value().getStringView());
  unsigned int group = extractor.subgroup();
  // get and regex
  std::regex extract_regex(extractor.regex());
  std::smatch regex_result;
  if (std::regex_match(value, regex_result, extract_regex)) {
    std::smatch::iterator submatch_it = regex_result.begin();
    for (unsigned i = 0; i < group; i++) {
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
    const std::map<std::string, std::string> &extractions, const json &context)
    : header_map_(header_map), extractions_(extractions), context_(context) {
  env_.add_callback(
      "header", 1, [this](Arguments args) {
        return header_callback(args);
      });
  env_.add_callback(
      "extraction", 1, [this](Arguments args) {
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

std::string TransformerInstance::render(const std::string &input) {
  return env_.render(input, context_);
}

Transformer::Transformer(
    const envoy::api::v2::filter::http::TransformationTemplate &transformation)
    : transformation_(transformation) {}

Transformer::~Transformer() {}

void Transformer::transform(Http::HeaderMap &header_map,
                            Buffer::Instance &body) {
  json json_body;
  if (body.length() > 0) {
    const std::string bodystring = body.toString();

    // parse the body as json
    json_body = json::parse(bodystring);
  }
  // get the extractions
  std::map<std::string, std::string> extractions;

  const auto &extractors = transformation_.extractors();

  for (auto it = extractors.begin(); it != extractors.end(); it++) {
    const std::string &name = it->first;
    const envoy::api::v2::filter::http::Extraction &extractor = it->second;
    if (transformation_.advanced_templates()) {
      extractions[name] = ExtractorUtil::extract(extractor, header_map);
    } else {
      std::string name_to_split = name;
      json *current = &json_body;
      for (size_t pos = name_to_split.find("."); pos != std::string::npos;
           pos = name_to_split.find(".")) {
        auto &&field_name = name_to_split.substr(0, pos);
        current = &(*current)[field_name];
        name_to_split.erase(0, pos + 1);
      }
      (*current)[name_to_split] = ExtractorUtil::extract(extractor, header_map);
    }
  }
  // start transforming!
  TransformerInstance instance(header_map, extractions, json_body);

  if (!transformation_.advanced_templates()) {
    instance.useDotNotation();
  }

  switch (transformation_.body_transformation_case()) {
  case envoy::api::v2::filter::http::TransformationTemplate::kBody: {
    const std::string &input = transformation_.body().text();
    auto output = instance.render(input);

    // remove content length, as we have new body.
    header_map.removeContentLength();
    // replace body
    body.drain(body.length());
    body.add(output);
    header_map.insertContentLength().value(body.length());
    break;
  }

  case envoy::api::v2::filter::http::TransformationTemplate::
      kMergeExtractorsToBody: {
    std::string output = json_body.dump();

    // remove content length, as we have new body.
    header_map.removeContentLength();
    // replace body
    body.drain(body.length());
    body.add(output);
    header_map.insertContentLength().value(body.length());
    break;
  }
  case envoy::api::v2::filter::http::TransformationTemplate::kPassthrough:
  case envoy::api::v2::filter::http::TransformationTemplate::
      BODY_TRANSFORMATION_NOT_SET: {
    break;
  }
  }

  // add headers
  const auto &headers = transformation_.headers();

  for (auto it = headers.begin(); it != headers.end(); it++) {
    std::string name = it->first;
    auto lkname = Http::LowerCaseString(std::move(name));
    const envoy::api::v2::filter::http::InjaTemplate &text = it->second;
    std::string output = instance.render(text.text());
    // remove existing header
    header_map.remove(lkname);
    // TODO(yuval-k): Do we need to support intentional empty headers?
    if (!output.empty()) {
      header_map.addCopy(lkname, output);
    }
  }
}

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
