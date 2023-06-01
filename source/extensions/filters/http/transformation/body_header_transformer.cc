#include "source/extensions/filters/http/transformation/body_header_transformer.h"

#include "source/common/http/headers.h"
#include "source/common/http/utility.h"

#include "nlohmann/json.hpp"

// For convenience
using json = nlohmann::json;

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

BodyHeaderTransformer::BodyHeaderTransformer(bool add_request_metadata, google::protobuf::BoolValue log_request_response_info)
    : Transformer(log_request_response_info), add_request_metadata_(add_request_metadata){}

void BodyHeaderTransformer::transform(
    Http::RequestOrResponseHeaderMap &header_map,
    Http::RequestHeaderMap *request_headers, Buffer::Instance &body,
    Http::StreamFilterCallbacks &) const {
  json json_body;
  if (body.length() > 0) {
    json_body["body"] = body.toString();
  }

  auto headers = std::map<std::string, std::string>{};
  // multi_value_headers will not be populated if add_request_metadata_ is false
  auto multi_value_headers = std::map<std::string, std::vector<std::string>>{};
  parse_headers(header_map, headers, multi_value_headers);
  json_body["headers"] = headers;

  if (add_request_metadata_) {
    if (request_headers == (&header_map)){
      // this is a request!
      json_body["multiValueHeaders"] = multi_value_headers;
      const Http::HeaderString& path = request_headers->Path()->value();
      absl::string_view query_string = Http::Utility::findQueryStringStart(path);
      absl::string_view path_view = path.getStringView();
      path_view.remove_suffix(query_string.length());
      if (query_string.size() > 0) {
        // remove the question mark
        query_string.remove_prefix(1);
      }
      json_body["queryString"] = query_string;
      json_body["httpMethod"] = request_headers->Method()->value().getStringView();
      json_body["path"] = path_view;

      auto query_string_parameters = std::map<std::string, std::string>{};
      auto multi_value_query_string_parameters = std::map<std::string, std::vector<std::string>>{};
      parse_query_string(query_string, query_string_parameters, multi_value_query_string_parameters);
      json_body["queryStringParameters"] = query_string_parameters;
      json_body["multiValueQueryStringParameters"] = multi_value_query_string_parameters;
    }
  }

  // remove content length, as we have new body.
  header_map.removeContentLength();
  // we know that the new content type is json:
  header_map.removeContentType();
  header_map.setReferenceContentType(
      Http::Headers::get().ContentTypeValues.Json);

  // replace body
  body.drain(body.length());
  body.add(json_body.dump());
  header_map.setContentLength(body.length());
}

void BodyHeaderTransformer::parse_headers(
  const Http::RequestOrResponseHeaderMap &header_map,
  std::map<std::string, std::string> &headers,
  std::map<std::string, std::vector<std::string>> &multi_value_headers) const {
    if (add_request_metadata_) {
      header_map.iterate(
        [&headers, &multi_value_headers](const Http::HeaderEntry& header) -> Http::HeaderMap::Iterate {
            auto header_key = std::string(header.key().getStringView());
            auto header_value = std::string(header.value().getStringView());
            // If there are more than one headers with the same key, use the last one
            auto existing_header_value = headers[header_key];
            headers[header_key] = header_value;

            // If there is no existing value, we don't need to do anything else
            if (existing_header_value.empty()) {
                return Http::HeaderMap::Iterate::Continue;
            }

            // If this is the second time we've seen this header, we need to add the first value to
            // multi_value_headers
            if (multi_value_headers[header_key].empty()) {
                multi_value_headers[header_key].emplace_back(existing_header_value);
            }

            // Add the current value to multi_value_headers
            multi_value_headers[header_key].emplace_back(header_value);
            return Http::HeaderMap::Iterate::Continue;
        });
    } else {
      header_map.iterate(
      [&headers](const Http::HeaderEntry &header) -> Http::HeaderMap::Iterate {
        headers[std::string(header.key().getStringView())] =
            std::string(header.value().getStringView());
        return Http::HeaderMap::Iterate::Continue;
      });
    }
}

void BodyHeaderTransformer::parse_query_string(
  absl::string_view query_string,
  std::map<std::string, std::string> &query_string_parameters,
  std::map<std::string, std::vector<std::string>> &multi_value_query_string_parameters) const {
    Envoy::Http::Utility::QueryParamsVector query_params = parse_parameters(query_string, 0);
    for (auto& pair : query_params) {
      if (query_string_parameters[pair.first].empty()) {
        query_string_parameters[pair.first] = pair.second;
      } else {
        auto existing_value = query_string_parameters[pair.first];
        query_string_parameters[pair.first] = pair.second;
        // handle multi value case
        if (multi_value_query_string_parameters[pair.first].empty()) {
          multi_value_query_string_parameters[pair.first] = std::vector<std::string>();
          multi_value_query_string_parameters[pair.first].emplace_back(existing_value);
        }
        multi_value_query_string_parameters[pair.first].emplace_back(pair.second);
      }
    }
}

// Modified version of Envoy::Http::Utility::parseParameters which supports
// multi-value query params
Envoy::Http::Utility::QueryParamsVector parse_parameters(absl::string_view data, size_t start) {
  Envoy::Http::Utility::QueryParamsVector params;

  while (start < data.size()) {
    size_t end = data.find('&', start);
    if (end == std::string::npos) {
      end = data.size();
    }
    absl::string_view param(data.data() + start, end - start);

    const size_t equal = param.find('=');
    if (equal != std::string::npos) {
      const auto param_name = StringUtil::subspan(data, start, start + equal);
      const auto param_value = StringUtil::subspan(data, start + equal + 1, end);
      auto pair = std::make_pair(
        Envoy::Http::Utility::PercentEncoding::decode(param_name),
        Envoy::Http::Utility::PercentEncoding::decode(param_value));
      params.push_back(pair);
    } else {
      auto pair = std::make_pair(StringUtil::subspan(data, start, end), "");
      params.push_back(pair);
    }

    start = end + 1;
  }

  return params;
}


} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
