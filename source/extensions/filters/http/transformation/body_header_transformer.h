#pragma once

#include <map>

#include "source/extensions/filters/http/transformation/transformer.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

class BodyHeaderTransformer : public Transformer {
public:
  BodyHeaderTransformer(bool add_request_metadata);
  void transform(Http::RequestOrResponseHeaderMap &map,
                 Http::RequestHeaderMap *request_headers,
                 Buffer::Instance &body,
                 Http::StreamFilterCallbacks &) const override;
  bool passthrough_body() const override { return false; };
  void parse_headers(const Http::RequestOrResponseHeaderMap &header_map,
                     std::map<std::string, std::string> &headers,
                     std::map<std::string, std::vector<std::string>> &multi_value_headers) const;
  void parse_query_string(absl::string_view query_string,
                        std::map<std::string, std::string> &query_string_parameters,
                        std::map<std::string, std::vector<std::string>> &multi_value_query_string_parameters) const;
private:
  bool add_request_metadata_{};

};

Envoy::Http::Utility::QueryParamsVector parse_parameters(absl::string_view data, size_t start);

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
