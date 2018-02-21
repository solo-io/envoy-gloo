#include <iterator> 
#include "common/common/macros.h"
#include "common/http/filter/transformer.h"

// For convenience
using namespace inja;
using json = nlohmann::json;

using namespace std::placeholders;

namespace Envoy {
namespace Http {
// TODO: move to common
const Envoy::Http::HeaderEntry *getHeader(const HeaderMap& header_map, std::string&& key) {
    auto lowerkey = LowerCaseString(std::move(key), true);

    const Envoy::Http::HeaderEntry *header_entry = header_map.get(lowerkey);
    if (! header_entry) {
        header_map.lookup(lowerkey, &header_entry);
    }
    return header_entry;
}

std::string ExtractorUtil::extract(const envoy::api::v2::filter::http::Extraction& extractor, const HeaderMap& header_map) {
    std::string headername = extractor.header();
    const Envoy::Http::HeaderEntry *header_entry = getHeader(header_map, std::move(headername));
    if (! header_entry) {
        return "";
    }

    std::string value = header_entry->value().c_str();
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

TransformerInstance::TransformerInstance(const HeaderMap& header_map, const std::map<std::string,std::string> &extractions, const json& context) :
header_map_(header_map), extractions_(extractions), context_(context) {
    env_.add_callback("header", 1, std::bind(&TransformerInstance::header_callback, this, _1 ,_2));
    env_.add_callback("extraction", 1, std::bind(&TransformerInstance::extracted_callback, this, _1 ,_2));
}

json TransformerInstance::header_callback(Parsed::Arguments args, json data) {
    std::string headername = env_.get_argument<std::string>(args, 0, data);
    const Envoy::Http::HeaderEntry *header_entry = getHeader(header_map_, std::move(headername));
    if (! header_entry) {
        return "";
    }
    return header_entry->value().c_str();
}

json TransformerInstance::extracted_callback(Parsed::Arguments args, json data) {
    std::string name = env_.get_argument<std::string>(args, 0, data);
    const auto value_it =  extractions_.find(name);
    if (value_it != extractions_.end()) {
        return value_it->second;
    }
    return "";
}

std::string TransformerInstance::render(std::string input) {
    return env_.render(input, context_);
}


Transformer::Transformer(const envoy::api::v2::filter::http::Transformation& transformation): transformation_(transformation) {
}

Transformer::~Transformer(){
}

void transform(HeaderMap& hedader_map, Buffer::Instance &body) {
    UNREFERENCED_PARAMETER(hedader_map);
    UNREFERENCED_PARAMETER(body);
    /*
    TODO:
        body to string
        body string to json

        for ever extraction,
         extract it and add it to the map

        For ever header in the template:
         run it through the extraction, and insert them to the header map;

        run the body through the extraction and update the incoming buffer 
        (i.e. drain and move)
    */

}




} // namespace Http
} // namespace Envoy
