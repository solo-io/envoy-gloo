#include "source/extensions/filters/http/transformation/inja_transformer.h"

#include <iterator>

#include "absl/strings/str_replace.h"

#include "source/common/buffer/buffer_impl.h"
#include "source/common/common/macros.h"
#include "source/common/common/regex.h"
#include "source/common/regex/regex.h"
#include "source/common/common/utility.h"
#include "source/common/config/metadata.h"
#include "source/common/common/empty_string.h"

#include "source/extensions/filters/http/solo_well_known_names.h"

extern char **environ;

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

struct BoolHeaderValues {
  const std::string trueString = "true";
  const std::string falseString = "false";
};
typedef ConstSingleton<BoolHeaderValues> BoolHeader;

// TODO: move to common
namespace {
const Http::HeaderMap::GetResult
getHeader(const Http::RequestOrResponseHeaderMap &header_map,
          const Http::LowerCaseString &key) {
  return header_map.get(key);
}

const Http::HeaderMap::GetResult
getHeader(const Http::RequestOrResponseHeaderMap &header_map,
          const std::string &key) {
  // use explicit constuctor so string is lowered
  auto lowerkey = Http::LowerCaseString(key);
  return getHeader(header_map, lowerkey);
}

} // namespace

Extractor::Extractor(const envoy::api::v2::filter::http::Extraction &extractor)
    : headername_(extractor.header()), body_(extractor.has_body()),
      group_(extractor.subgroup()),
      extract_regex_(Solo::Regex::Utility::parseStdRegex(extractor.regex())),
      replacement_text_(extractor.has_replacement_text() ? std::make_optional(extractor.replacement_text().value()) : std::nullopt),
      mode_(extractor.mode()) {
  // mark count == number of sub groups, and we need to add one for match number
  // 0 so we test for < instead of <= see:
  // http://www.cplusplus.com/reference/regex/basic_regex/mark_count/
  if (extract_regex_.mark_count() < group_) {
    throw EnvoyException(
        fmt::format("group {} requested for regex with only {} sub groups",
                    group_, extract_regex_.mark_count()));
  }

  switch (mode_) {
    case ExtractionApi::EXTRACT:
      break;
    case ExtractionApi::SINGLE_REPLACE:
      if (!replacement_text_.has_value()) {
        throw EnvoyException("SINGLE_REPLACE mode set but no replacement text provided");
      }
      break;
    case ExtractionApi::REPLACE_ALL:
      if (!replacement_text_.has_value()) {
        throw EnvoyException("REPLACE_ALL mode set but no replacement text provided");
      }
      if (group_ != 0) {
        throw EnvoyException("REPLACE_ALL mode set but subgroup is not 0");
      }
      break;
    default:
      throw EnvoyException("Unknown mode");
  }
}

absl::string_view
Extractor::extract(Http::StreamFilterCallbacks &callbacks,
                   const Http::RequestOrResponseHeaderMap &header_map,
                   GetBodyFunc &body) const {
  if (body_) {
    const std::string &string_body = body();
    absl::string_view sv(string_body);
    return extractValue(callbacks, sv);
  } else {
    const Http::HeaderMap::GetResult header_entries = getHeader(header_map, headername_);
    if (header_entries.empty()) {
      return "";
    }
    return extractValue(callbacks, header_entries[0]->value().getStringView());
  }
}

std::string
Extractor::extractDestructive(Http::StreamFilterCallbacks &callbacks,
                   const Http::RequestOrResponseHeaderMap &header_map,
                   GetBodyFunc &body) const {
  // determines which destructive extraction function to call based on the mode
  auto extractFunc = [&](Http::StreamFilterCallbacks& callbacks, absl::string_view sv) {
    switch (mode_) {
      case ExtractionApi::SINGLE_REPLACE:
        return replaceIndividualValue(callbacks, sv);
      case ExtractionApi::REPLACE_ALL:
        return replaceAllValues(callbacks, sv);
      default:
        // Handle unknown mode
        throw EnvoyException("Cannot use extractDestructive with unsupported mode");
    }
  };

  if (body_) {
    const std::string &string_body = body();
    absl::string_view sv(string_body);
    return extractFunc(callbacks, sv);
  } else {
    const Http::HeaderMap::GetResult header_entries = getHeader(header_map, headername_);
    if (header_entries.empty()) {
      return "";
    }
    const auto &header_value = header_entries[0]->value().getStringView();
    return extractFunc(callbacks, header_value);
  }
}

absl::string_view
Extractor::extractValue(Http::StreamFilterCallbacks &callbacks,
                        absl::string_view value) const {
  // get and regex
  std::match_results<absl::string_view::const_iterator> regex_result;
  if (std::regex_match(value.begin(), value.end(), regex_result,
                       extract_regex_)) {
    if (group_ >= regex_result.size()) {
      // this should never happen as we test this in the ctor.
      ASSERT("no such group in the regex");
      ENVOY_STREAM_LOG(debug, "invalid group specified for regex", callbacks);
      return "";
    }
    const auto &sub_match = regex_result[group_];
    return absl::string_view(sub_match.first, sub_match.length());
  } else {
    ENVOY_STREAM_LOG(debug, "extractor regex did not match input", callbacks);
  }
  return "";
}

// Match a regex against the input value and replace the matched subgroup with the replacement_text_ value
std::string
Extractor::replaceIndividualValue(Http::StreamFilterCallbacks &callbacks,
                                  absl::string_view value) const {
  std::match_results<absl::string_view::const_iterator> regex_result;

  // if there are no matches, return the original input value
  if (!std::regex_search(value.begin(), value.end(), regex_result, extract_regex_)) {
    ENVOY_STREAM_LOG(debug, "replaceIndividualValue: extractor regex did not match input. Returning input", callbacks);
    return std::string(value.begin(), value.end());
  }

  // if the subgroup specified is greater than the number of subgroups in the regex, return the original input value
  if (group_ >= regex_result.size()) {
    // this should never happen as we test this in the ctor.
    ASSERT("no such group in the regex");
    ENVOY_STREAM_LOG(debug, "replaceIndividualValue: invalid group specified for regex. Returning input", callbacks);
    return std::string(value.begin(), value.end());
  }

  // if the regex doesn't match the entire input value, return the original input value
  if (regex_result[0].length() != long(value.length())) {
    ENVOY_STREAM_LOG(debug, "replaceIndividualValue: Regex did not match entire input value. This is not allowed in SINGLE_REPLACE mode. Returning input", callbacks);
    return std::string(value.begin(), value.end());
  }

  // Create a new string with the maximum possible length after replacement
  auto max_possible_length = value.length() + replacement_text_.value().length();
  std::string replaced;
  replaced.reserve(max_possible_length);

  auto subgroup_start = regex_result[group_].first;
  auto subgroup_end = regex_result[group_].second;

  // Copy the initial part of the string until the match
  replaced.assign(value.begin(), subgroup_start);

  // Append the replacement text
  replaced += replacement_text_.value();

  // Append the remaining part of the string after the match
  replaced.append(subgroup_end, value.end());

  return replaced;
}

// Match a regex against the input value and replace all instances of the regex with the replacement_text_ value
std::string
Extractor::replaceAllValues(Http::StreamFilterCallbacks&,
                            absl::string_view value) const {
  std::string input(value.begin(), value.end());
  std::string replaced;

  // replace all instances of the regex in the input value with the replacement_text_ value
  return std::regex_replace(input, extract_regex_, replacement_text_.value(), std::regex_constants::match_not_null);
}

// A TransformerInstance is constructed by the InjaTransformer constructor at config time
// on the main thread. It access thread-local storage which is populated during the
// InjaTransformer::transform method call, which happens on the request path on any
// given worker thread.
TransformerInstance::TransformerInstance(ThreadLocal::Slot &tls, Envoy::Random::RandomGenerator &rng)
    : tls_(tls), rng_(rng) {
  env_.add_callback("header", 1,
                    [this](Arguments &args) { return header_callback(args); });
  env_.add_callback("request_header", 1, [this](Arguments &args) {
    return request_header_callback(args);
  });
  env_.add_callback("extraction", 1, [this](Arguments &args) {
    return extracted_callback(args);
  });
  env_.add_callback("context", 0, [this](Arguments &) { return *tls_.getTyped<ThreadLocalTransformerContext>().context_; });
  env_.add_callback("body", 0, [this](Arguments &) { return (*tls_.getTyped<ThreadLocalTransformerContext>().body_)(); });
  env_.add_callback("env", 1, [this](Arguments &args) { return env(args); });
  env_.add_callback("clusterMetadata", 1, [this](Arguments &args) {
    return cluster_metadata_callback_deprecated(args);
  });
  env_.add_callback("cluster_metadata", [this](Arguments &args) {
    return cluster_metadata_callback(args);
  });
  env_.add_callback("dynamic_metadata", [this](Arguments &args) {
    return dynamic_metadata_callback(args);
  });
  env_.add_callback("host_metadata", [this](Arguments &args) {
    return host_metadata_callback(args);
  });
  env_.add_callback("base64_encode", 1, [this](Arguments &args) {
    return base64_encode_callback(args);
  });
  env_.add_callback("base64url_encode", 1, [this](Arguments &args) {
    return base64url_encode_callback(args);
  });
  env_.add_callback("base64_decode", 1, [this](Arguments &args) {
    return base64_decode_callback(args);
  });
  env_.add_callback("base64url_decode", 1, [this](Arguments &args) {
    return base64url_decode_callback(args);
  });
  // substring can be called with either two or three arguments --
  // the first argument is the string to be modified, the second is the start position
  // of the substring, and the optional third argument is the length of the substring.
  // If the third argument is not provided, the substring will extend to the end of the string.
  env_.add_callback("substring", 2, [this](Arguments &args) {
    return substring_callback(args);
  });
  env_.add_callback("substring", 3, [this](Arguments &args) {
    return substring_callback(args);
  });
  env_.add_callback("replace_with_random", 2, [this](Arguments &args) {
    return replace_with_random_callback(args);
  });
  env_.add_callback("raw_string", 1, [this](Arguments &args) {
    return raw_string_callback(args);
  });
  env_.add_callback("word_count", 1, [](Arguments &args) {
    return word_count_callback(args);
  });
}



json TransformerInstance::cluster_metadata_callback_deprecated(const inja::Arguments &args) const {
      const auto& ctx = tls_.getTyped<ThreadLocalTransformerContext>();
      const std::string &key = args.at(0)->get_ref<const std::string &>();

      if (!ctx.cluster_metadata_) {
        return "";
      }

      const ProtobufWkt::Value &value = Envoy::Config::Metadata::metadataValue(
          ctx.cluster_metadata_, SoloHttpFilterNames::get().Transformation, key);

      switch (value.kind_case()) {
      case ProtobufWkt::Value::kStringValue: {
        return value.string_value();
        break;
      }
      case ProtobufWkt::Value::kNumberValue: {
        return value.number_value();
        break;
      }
      case ProtobufWkt::Value::kBoolValue: {
        const std::string &stringval = value.bool_value()
                                          ? BoolHeader::get().trueString
                                          : BoolHeader::get().falseString;
        return stringval;
        break;
      }
      case ProtobufWkt::Value::kListValue: {
        const auto &listval = value.list_value().values();
        if (listval.size() == 0) {
          break;
        }

        // size is not zero, so this will work
        auto it = listval.begin();
        std::stringstream ss;

        auto addValue = [&ss, &it] {
          const ProtobufWkt::Value &value = *it;

          switch (value.kind_case()) {
          case ProtobufWkt::Value::kStringValue: {
            ss << value.string_value();
            break;
          }
          case ProtobufWkt::Value::kNumberValue: {
            ss << value.number_value();
            break;
          }
          case ProtobufWkt::Value::kBoolValue: {
            ss << (value.bool_value() ? BoolHeader::get().trueString
                                      : BoolHeader::get().falseString);
            break;
          }
          default:
            break;
          }
        };

        addValue();

        for (it++; it != listval.end(); it++) {
          ss << ",";
          addValue();
        }
        return ss.str();
      }
      default: {
        break;
      }
      }
      return "";
}

json TransformerInstance::header_callback(const inja::Arguments &args) const {
  const std::string &headername = args.at(0)->get_ref<const std::string &>();
  const auto& ctx = tls_.getTyped<ThreadLocalTransformerContext>();
  const Http::HeaderMap::GetResult header_entries = getHeader(*ctx.header_map_, headername);
  if (header_entries.empty()) {
    return "";
  }
  return std::string(header_entries[0]->value().getStringView());
}

json TransformerInstance::request_header_callback(
    const inja::Arguments &args) const {
  const auto& ctx = tls_.getTyped<ThreadLocalTransformerContext>();
  if (ctx.request_headers_ == nullptr) {
    return "";
  }
  const std::string &headername = args.at(0)->get_ref<const std::string &>();
  const Http::HeaderMap::GetResult header_entries =
      getHeader(*ctx.request_headers_, headername);
  if (header_entries.empty()) {
    return "";
  }
  return std::string(header_entries[0]->value().getStringView());
}

json TransformerInstance::extracted_callback(const inja::Arguments &args) const {
  const auto& ctx = tls_.getTyped<ThreadLocalTransformerContext>();
  const std::string &name = args.at(0)->get_ref<const std::string &>();
  const auto value_it = ctx.extractions_->find(name);
  if (value_it != ctx.extractions_->end()) {
    return value_it->second;
  }

  const auto destructive_value_it = ctx.destructive_extractions_->find(name);
  if (destructive_value_it != ctx.destructive_extractions_->end()) {
    return destructive_value_it->second;
  }
  return "";
}

json TransformerInstance::env(const inja::Arguments &args) const {
  const auto& ctx = tls_.getTyped<ThreadLocalTransformerContext>();
  const std::string &key = args.at(0)->get_ref<const std::string &>();
  auto it = ctx.environ_->find(key);
  if (it != ctx.environ_->end()) {
    return it->second;
  }
  return "";
}

json TransformerInstance::host_metadata_callback(const inja::Arguments &args) const {
  const auto& ctx = tls_.getTyped<ThreadLocalTransformerContext>();
  if (!ctx.endpoint_metadata_) {
    return "";
  }
  return parse_metadata(ctx.endpoint_metadata_.get(), args);
}
json TransformerInstance::dynamic_metadata_callback(const inja::Arguments &args) const {
  const auto& ctx = tls_.getTyped<ThreadLocalTransformerContext>();
  if (!ctx.dynamic_metadata_) {
    return "";
  }
  return parse_metadata(ctx.dynamic_metadata_, args);
}

json TransformerInstance::cluster_metadata_callback(const inja::Arguments &args) const {
  const auto& ctx = tls_.getTyped<ThreadLocalTransformerContext>();
  if (!ctx.cluster_metadata_) {
    return "";
  }
  return parse_metadata(ctx.cluster_metadata_, args);
}

json TransformerInstance::parse_metadata(const envoy::config::core::v3::Metadata* metadata,
                                                  const inja::Arguments &args) {

  // If a 2nd args is provided, use it as the filter
  const std::string &filter = args.size() > 1 ? args.at(1)->get_ref<const std::string &>() : SoloHttpFilterNames::get().Transformation;
  const std::string &key = args.at(0)->get_ref<const std::string &>();

  const ProtobufWkt::Value &value = Envoy::Config::Metadata::metadataValue(
      metadata, filter, key);

  switch (value.kind_case()) {
  case ProtobufWkt::Value::kStringValue: {
    return value.string_value();
    break;
  }
  case ProtobufWkt::Value::kNumberValue: {
    return value.number_value();
    break;
  }
  case ProtobufWkt::Value::kBoolValue: {
    const std::string &stringval = value.bool_value()
                                       ? BoolHeader::get().trueString
                                       : BoolHeader::get().falseString;
    return stringval;
    break;
  }
  case ProtobufWkt::Value::kStructValue: {
    std::string output;
    auto status = ProtobufUtil::MessageToJsonString(value.struct_value(), &output);
    return output;
    break;
  }
  case ProtobufWkt::Value::kListValue: {
    std::string output;
    auto status = ProtobufUtil::MessageToJsonString(value.list_value(), &output);
    return output;
    break;
  }
  default: {
    break;
  }
  }
  return "";
}

json TransformerInstance::base64_encode_callback(const inja::Arguments &args) const {
  const std::string &input = args.at(0)->get_ref<const std::string &>();
  return Base64::encode(input.c_str(), input.length());
}

json TransformerInstance::base64url_encode_callback(const inja::Arguments &args) const {
  const std::string &input = args.at(0)->get_ref<const std::string &>();
  return Base64Url::encode(input.c_str(), input.length());
}

json TransformerInstance::base64_decode_callback(const inja::Arguments &args) const {
  const std::string &input = args.at(0)->get_ref<const std::string &>();
  return Base64::decode(input);
}

json TransformerInstance::base64url_decode_callback(const inja::Arguments &args) const {
  const std::string &input = args.at(0)->get_ref<const std::string &>();
  return Base64Url::decode(input);
}

json TransformerInstance::word_count_callback(const inja::Arguments &args) {
  return json_word_count(args.at(0));
}

int TransformerInstance::json_word_count(const nlohmann::json* input)  {
  if (input->is_string()) {
    const std::string &input_string = input->get_ref<const std::string &>();
    return word_count(input_string);
  } else if (input->is_array()) {
    int total_word_count = 0;
    const auto &input_array = input->get_ref<const std::vector<json> &>();
    for (auto & element : input_array) {
      total_word_count += json_word_count(&element);
    }
    return total_word_count;
  } else if (input->is_object()) {
    int total_word_count = 0;
    const auto element_obj = input->get_ref<const json::object_t &>();
    for (auto & [key, value] : element_obj) {
      total_word_count += word_count(key);
      total_word_count += json_word_count(&value);
    }
    return total_word_count;
  } else if (input->is_number() || input->is_boolean()) {
    // Booleans and numbers are constant
    return 1;
  }
  return 0;
}

int TransformerInstance::word_count(const std::string& input_string)  {
    unsigned long ctr = 0; // Initializing a counter variable to count words

    // Advance through all spaces at the beginning


    unsigned long first_char = 0;
    for (unsigned long x = 0; x < input_string.length(); x++) {
      // https://en.cppreference.com/w/cpp/string/byte/isspace
      if (!isspace(input_string[x] )){
        first_char = x;
        break;
      }
    }

    // Loop through the string and count spaces to determine words
    bool in_white_space = false;
    for (unsigned long x = first_char; x < input_string.length(); x++) {
      // https://en.cppreference.com/w/cpp/string/byte/isspace
      if (isspace(input_string[x] )){ // Checking for spaces to count words
        if (!in_white_space){
          ctr++; // Increment the counter for each new "word"
        }
        in_white_space = true;
      } else{
        in_white_space = false;
      }
    }
    // Return the count of words by adding 1 to the total number of spaces
    // (plus 1 for the last word without a trailing space
    // unless it ends with a space 
    if (isspace(input_string[input_string.length() - 1] )){
      return ctr;
    } else {
      return ctr + 1;
    }
}


// return a substring of the input string, starting at the start position
// and extending for length characters. If length is not provided, the
// substring will extend to the end of the string.
json TransformerInstance::substring_callback(const inja::Arguments &args) const {
  // get first argument, which is the string to be modified
  const std::string &input = args.at(0)->get_ref<const std::string &>();

  // try to get second argument (start position) as an int64_t
  int start = 0;
  try {
    start = args.at(1)->get_ref<const int64_t &>();
  } catch (const std::exception &e) {
    // if it can't be converted to an int64_t, return an empty string
    return "";
  }

  // try to get optional third argument (length) as an int64_t
  int64_t substring_len = -1;
  if (args.size() == 3) {
    try {
      substring_len = args.at(2)->get_ref<const int64_t &>();
    } catch (const std::exception &e) {
      // if it can't be converted to an int64_t, return an empty string
      return "";
    }
  }
  const int64_t input_len = input.length();

  // if start is negative, or start is greater than the length of the string, return empty string
  if (start < 0 ||  start >= input_len) {
    return "";
  }

  // if supplied substring_len is <= 0 or start + substring_len is greater than the length of the string,
  // return the substring from start to the end of the string
  if (substring_len <= 0 || start + substring_len > input_len) {
    return input.substr(start);
  }

  // otherwise, return the substring from start to start + len
  return input.substr(start, substring_len);
}

json TransformerInstance::replace_with_random_callback(const inja::Arguments &args) {
    // first argument: string to modify
  const std::string &source = args.at(0)->get_ref<const std::string &>();
    // second argument: pattern to be replaced
  const std::string &to_replace = args.at(1)->get_ref<const std::string &>();

  return
    absl::StrReplaceAll(source, {{to_replace, absl::StrCat(random_for_pattern(to_replace))}});
}

std::string& TransformerInstance::random_for_pattern(const std::string& pattern) {
  auto found = pattern_replacements_.find(pattern);
  if (found == pattern_replacements_.end()) {
    // generate 128 bit long random number
    uint64_t random[2];
    uint64_t high = rng_.random();
    uint64_t low = rng_.random();
    random[0] = low;
    random[1] = high;
    // and convert it to a base64-encoded string with no padding
    pattern_replacements_.insert({pattern, Base64::encode(reinterpret_cast<char *>(random), 16, false)});
    return pattern_replacements_[pattern];
  }
  return found->second;
}

json TransformerInstance::raw_string_callback(const inja::Arguments &args) const {
  // inja::Arguments is a std::vector<const json *>, so we can get the json
  // value from the args directly. We are guaranteed to have exactly one argument
  // because Inja will throw a Parser error in any other case.
  // https://github.com/pantor/inja/blob/v3.4.0/include/inja/parser.hpp#L228-L231
  const auto& input = args.at(0);

  // make sure to bail if we're not working with a raw string value
  if(!input->is_string()) {
      return input->get_ref<const std::string&>();
  }

  auto val = input->dump();

  // This block makes it such that a template must have surrounding " characters
  // around the raw string. This is reasonable since we expect the value we get out of the
  // context (body) to be placed in exactly as-is. HOWEVER, the behavior of the jinja
  // filter is such that the quotes added by .dumps() are left in. For that reason,
  // this callback is NOT named to_json to avoid confusion with that behavior.

  // strip the leading and trailing " characters that are added by dump()
  // if C++20 is adopted, val.starts_with and val.ends_with would clean this up a bit
  val = val.substr(0,1) == "\"" && val.substr(val.length()-1,1) == "\""
      ? val.substr(1, val.length()-2)
      : val;
  return val;
}

// parse calls Inja::Environment::parse which uses non-const references to member
// data fields. This method is NOT SAFE to call outside of the InjaTransformer
// constructor since doing so could cause Inja::Environment member fields to be
// modified by multiple threads at runtime.
inja::Template TransformerInstance::parse(std::string_view input) {
    return env_.parse(input);
}

std::string TransformerInstance::render(const inja::Template &input) {
  // inja can't handle context that are not objects correctly, so we give it an
  // empty object in that case
  const auto& ctx = tls_.getTyped<ThreadLocalTransformerContext>();
  if (ctx.context_->is_object()) {
    return env_.render(input, *ctx.context_);
  } else {
    return env_.render(input, {});
  }
}

// An InjaTransformer is constructed on initialization on the main thread
InjaTransformer::InjaTransformer(const TransformationTemplate &transformation,
                                 Envoy::Random::RandomGenerator &rng,
                                 google::protobuf::BoolValue log_request_response_info,
                                 ThreadLocal::SlotAllocator &tls)
    : Transformer(log_request_response_info),
      advanced_templates_(transformation.advanced_templates()),
      passthrough_body_(transformation.has_passthrough()),
      parse_body_behavior_(transformation.parse_body_behavior()),
      ignore_error_on_parse_(transformation.ignore_error_on_parse()),
      escape_characters_(transformation.escape_characters()),
      tls_(tls.allocateSlot()),
      instance_(std::make_unique<TransformerInstance>(*tls_, rng)) {
  if (advanced_templates_) {
    instance_->set_element_notation(inja::ElementNotation::Pointer);
  }

  instance_->set_escape_strings(escape_characters_);

  tls_->set([](Event::Dispatcher&) -> ThreadLocal::ThreadLocalObjectSharedPtr {
          return std::make_shared<ThreadLocalTransformerContext>();
  });

  const auto &extractors = transformation.extractors();
  for (auto it = extractors.begin(); it != extractors.end(); it++) {
    extractors_.emplace_back(std::make_pair(it->first, it->second));
  }
  const auto &headers = transformation.headers();
  for (auto it = headers.begin(); it != headers.end(); it++) {
    Http::LowerCaseString header_name(it->first);
    try {
      headers_.emplace_back(std::make_pair(std::move(header_name),
                                           instance_->parse(it->second.text())));
    } catch (const std::exception &e) {
      throw EnvoyException(fmt::format(
          "Failed to parse header template '{}': {}", it->first, e.what()));
    }
  }
  const auto &headers_to_remove = transformation.headers_to_remove();
  for (auto idx : headers_to_remove) {
    Http::LowerCaseString header_name(idx);
    try {
      headers_to_remove_.push_back(header_name);
    } catch (const std::exception &e) {
      throw EnvoyException(fmt::format(
          "Failed to parse header to remove '{}': {}", idx, e.what()));
      }
  }
  const auto &headers_to_append = transformation.headers_to_append();
  for (auto idx = 0; idx < transformation.headers_to_append_size(); idx++) {
    const auto &it = headers_to_append.Get(idx);
    Http::LowerCaseString header_name(it.key());
    try {
      headers_to_append_.emplace_back(std::make_pair(std::move(header_name),
                                           instance_->parse(it.value().text())));
    } catch (const std::exception &e) {
      throw EnvoyException(fmt::format(
          "Failed to parse header template '{}': {}", it.key(), e.what()));
    }
  }
  const auto &dynamic_metadata_values =
      transformation.dynamic_metadata_values();
  for (auto it = dynamic_metadata_values.begin();
       it != dynamic_metadata_values.end(); it++) {
    try {
      DynamicMetadataValue dynamicMetadataValue;
      dynamicMetadataValue.namespace_ = it->metadata_namespace();
      if (dynamicMetadataValue.namespace_.empty()) {
        dynamicMetadataValue.namespace_ =
            SoloHttpFilterNames::get().Transformation;
      }
      dynamicMetadataValue.key_ = it->key();
      dynamicMetadataValue.template_ = instance_->parse(it->value().text());
      dynamicMetadataValue.parse_json_ = it->json_to_proto();
      dynamic_metadata_.emplace_back(std::move(dynamicMetadataValue));
    } catch (const std::exception &e) {
      throw EnvoyException(fmt::format(
          "Failed to parse header template '{}': {}", it->key(), e.what()));
    }
  }

  switch (transformation.body_transformation_case()) {
  case TransformationTemplate::kBody: {
    try {
      body_template_.emplace(instance_->parse(transformation.body().text()));
    } catch (const std::exception &e) {
      throw EnvoyException(
          fmt::format("Failed to parse body template {}", e.what()));
    }
    break;
  }
  case TransformationTemplate::kMergeExtractorsToBody: {
    merged_extractors_to_body_ = true;
    break;
  }
  case TransformationTemplate::kMergeJsonKeys: {
    if (transformation.parse_body_behavior() == TransformationTemplate::DontParse) {
      throw EnvoyException("MergeJsonKeys requires parsing the body");
    }
    try {
      for (const auto &named_extractor : transformation.merge_json_keys().json_keys()) {
        merge_templates_.emplace_back(std::make_tuple(named_extractor.first, named_extractor.second.override_empty(), instance_->parse(named_extractor.second.tmpl().text())));
      }

    } catch (const std::exception &e) {
      throw EnvoyException(
          fmt::format("Failed to parse merge_body_key template {}", e.what()));
    }
    break;
  }
  case TransformationTemplate::kPassthrough:
    break;
  case TransformationTemplate::BODY_TRANSFORMATION_NOT_SET: {
    break;
  }
  }

  // parse environment
  for (char **env = environ; *env != 0; env++) {
    std::string current_env(*env);
    size_t equals = current_env.find("=");
    if (equals > 0) {
      std::string key = current_env.substr(0, equals);
      std::string value = current_env.substr(equals + 1);
      environ_[key] = value;
    }
  }
}

InjaTransformer::~InjaTransformer() {}

// transform is called on the request path, and may be executed on any worker thread.
// it must be thread-safe. note that calling instance_->parse is NOT THREAD SAFE
// and MUST NOT be done from this method.
void InjaTransformer::transform(Http::RequestOrResponseHeaderMap &header_map,
                                Http::RequestHeaderMap *request_headers,
                                Buffer::Instance &body,
                                Http::StreamFilterCallbacks &callbacks) const {
  absl::optional<std::string> string_body;
  GetBodyFunc get_body = [&string_body, &body]() -> const std::string & {
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
        } catch (const std::exception &) {
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
  std::unordered_map<std::string, std::string> destructive_extractions;
  
  if (advanced_templates_) {
    auto extractions_size = 0;
    auto destructive_extractions_size = 0;
    for (const auto &named_extractor : extractors_) {
      switch(named_extractor.second.mode()) {
        case ExtractionApi::REPLACE_ALL:
        case ExtractionApi::SINGLE_REPLACE: {
          destructive_extractions_size++;
          break;
        }
        case ExtractionApi::EXTRACT: {
          extractions_size++;
          break;
        }
        default: {
          PANIC_DUE_TO_CORRUPT_ENUM
        }
      }
    }

    extractions.reserve(extractions_size);
    destructive_extractions.reserve(destructive_extractions_size);
  }

  for (const auto &named_extractor : extractors_) {
    const std::string &name = named_extractor.first;
    
    // prepare variables for non-advanced_templates_ scenario
    absl::string_view name_to_split;
    json* current = nullptr;
    if (!advanced_templates_) {
      name_to_split = name;
      current = &json_body;
      for (size_t pos = name_to_split.find("."); pos != std::string::npos;
           pos = name_to_split.find(".")) {
        auto &&field_name = name_to_split.substr(0, pos);
        current = &(*current)[std::string(field_name)];
        name_to_split = name_to_split.substr(pos + 1);
      }
    }

    switch(named_extractor.second.mode()) {
      case ExtractionApi::REPLACE_ALL:
      case ExtractionApi::SINGLE_REPLACE: {
        if (advanced_templates_) {
          destructive_extractions[name] = named_extractor.second.extractDestructive(callbacks, header_map, get_body);
        } else {
          (*current)[std::string(name_to_split)] = named_extractor.second.extractDestructive(callbacks, header_map, get_body);
        }
        break;
      }
      case ExtractionApi::EXTRACT: {
        if (advanced_templates_) {
          extractions[name] = named_extractor.second.extract(callbacks, header_map, get_body);
        } else {
          (*current)[std::string(name_to_split)] = named_extractor.second.extract(callbacks, header_map, get_body);
        }
        break;
      }
      default: {
        PANIC_DUE_TO_CORRUPT_ENUM
      }
    }
  }

  // get cluster metadata
  const envoy::config::core::v3::Metadata *cluster_metadata{};
  Upstream::ClusterInfoConstSharedPtr ci = callbacks.clusterInfo();
  if (ci.get()) {
    cluster_metadata = &ci->metadata();
  }

  // get cluster metadata
  const envoy::config::core::v3::Metadata *dynamic_metadata{};
  dynamic_metadata = &callbacks.streamInfo().dynamicMetadata();

  Envoy::Upstream::MetadataConstSharedPtr endpoint_metadata{};
  // If there is a value we're in a upstream filter
  if (callbacks.upstreamCallbacks().has_value()) {
    auto &upstream_callbacks = callbacks.upstreamCallbacks().value().get();
    // Double check that upstream_host exists as if the wait_filter isn't properly setup
    // this can segfault
    if (upstream_callbacks.upstreamStreamInfo().upstreamInfo()->upstreamHost()) {
      endpoint_metadata = upstream_callbacks.upstreamStreamInfo().upstreamInfo()->upstreamHost()->metadata();
    }
  }

  
  // now that we have gathered all of the request-specific transformation data,
  // get the reference to the worker thread's local transformer context and
  // set the fields
  auto& typed_tls_data = tls_->getTyped<ThreadLocalTransformerContext>();
  typed_tls_data.header_map_ = &header_map;
  typed_tls_data.request_headers_ = request_headers;
  typed_tls_data.body_ = &get_body;
  typed_tls_data.extractions_ = &extractions;
  typed_tls_data.destructive_extractions_ = &destructive_extractions;
  typed_tls_data.context_ = &json_body;
  typed_tls_data.environ_ = &environ_;
  typed_tls_data.cluster_metadata_ = cluster_metadata;
  typed_tls_data.dynamic_metadata_ = dynamic_metadata;
  typed_tls_data.endpoint_metadata_ = endpoint_metadata;


  // Body transform:
  absl::optional<Buffer::OwnedImpl> maybe_body;

  if (body_template_.has_value()) {
    std::string output = instance_->render(body_template_.value());
    maybe_body.emplace(output);
  } else if (merged_extractors_to_body_) {
    std::string output = json_body.dump();
    maybe_body.emplace(output);
  } else if (!merge_templates_.empty()) {

    for (const auto &merge_template : merge_templates_) {
      const std::string &name = std::get<0>(merge_template);
      
      // prepare variables for non-advanced_templates_ scenario
      absl::string_view name_to_split;
      json* current = nullptr;
      // if (!advanced_templates_) {
      name_to_split = name;
      current = &json_body;
      for (size_t pos = name_to_split.find("."); pos != std::string::npos;
          pos = name_to_split.find(".")) {
        auto &&field_name = name_to_split.substr(0, pos);
        current = &(*current)[std::string(field_name)];
        name_to_split = name_to_split.substr(pos + 1);
      }
      const auto rendered = instance_->render(std::get<2>(merge_template));
      // Do not overwrite with empty unless specified
      if (rendered.size() > 0 || std::get<1>(merge_template)) {
        try {
          auto rendered_json = json::parse(rendered);
          (*current)[std::string(name_to_split)] = rendered_json;
        } catch (const std::exception &e) {
          ASSERT("failed to parse merge_json_key output");
          (*current)[std::string(name_to_split)] = rendered;
        }
      }
      // }
    }
    std::string output = json_body.dump();
    maybe_body.emplace(output);
  }

  // DynamicMetadata transform:
  for (const auto &templated_dynamic_metadata : dynamic_metadata_) {
    std::string output = instance_->render(templated_dynamic_metadata.template_);
    if (!output.empty()) {
      if (templated_dynamic_metadata.parse_json_) {
        // Need to check if number
        ProtobufWkt::Value value_obj;
        auto status = ProtobufUtil::JsonStringToMessage(output, &value_obj);
        if (status.ok()) {
          ProtobufWkt::Struct struct_obj;
          (*struct_obj.mutable_fields())[templated_dynamic_metadata.key_] = value_obj;
          callbacks.streamInfo().setDynamicMetadata(
              templated_dynamic_metadata.namespace_, struct_obj);
        } else {
          ProtobufWkt::Struct strct(
              MessageUtil::keyValueStruct(templated_dynamic_metadata.key_, output));
          callbacks.streamInfo().setDynamicMetadata(
              templated_dynamic_metadata.namespace_, strct);
        }
      } else {
        ProtobufWkt::Struct strct(
            MessageUtil::keyValueStruct(templated_dynamic_metadata.key_, output));
        callbacks.streamInfo().setDynamicMetadata(
            templated_dynamic_metadata.namespace_, strct);
      }

    }
  }

  // Headers transform:
  for (const auto &templated_header : headers_) {
    std::string output = instance_->render(templated_header.second);
    // remove existing header
    header_map.remove(templated_header.first);
    // TODO(yuval-k): Do we need to support intentional empty headers?
    if (!output.empty()) {
      // we can add the key as reference as the headers_ lifetime is as the
      // route's
      header_map.addReferenceKey(templated_header.first, output);
    }
  }

  for (const auto &header_to_remove : headers_to_remove_) {
    header_map.remove(header_to_remove);
  }

  // Headers to Append Values transform:
  for (const auto &templated_header : headers_to_append_) {
    std::string output = instance_->render(templated_header.second);
    if (!output.empty()) {
      // we can add the key as reference as the headers_to_append_ lifetime is as the
      // route's
      // don't remove headers that already exist
      header_map.addReferenceKey(templated_header.first, output);
    }
  }

  // replace body. we do it here so that headers and dynamic metadata have the
  // original body.
  if (maybe_body.has_value()) {
    // remove content length, as we have new body.
    header_map.removeContentLength();
    // replace body
    body.drain(body.length());
    // prepend is used because it doesn't copy, it drains maybe_body
    body.prepend(maybe_body.value());
    header_map.setContentLength(body.length());
  }
}

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
