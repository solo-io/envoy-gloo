#include "source/extensions/filters/http/transformation/body_header_transformer.h"

#include "body_header_transformer.h"
#include "source/common/http/headers.h"
#include "source/common/http/utility.h"

#include "nlohmann/json.hpp"
#include "transformer.h"
#include <memory>

// For convenience
using json = nlohmann::json;

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

json extractBodyAndHeaders(
    Http::RequestOrResponseHeaderMap &request_headers, Buffer::Instance &body) {
  json json_body;
  if (body.length() > 0) {
    json_body["body"] = body.toString();
  }

  json &headers = json_body["headers"];
  request_headers.iterate(
      [&headers](const Http::HeaderEntry &header) -> Http::HeaderMap::Iterate {
        headers[std::string(header.key().getStringView())] =
            std::string(header.value().getStringView());
        return Http::HeaderMap::Iterate::Continue;
      });
  return json_body;
}

void mutate(Http::RequestOrResponseHeaderMap &headers, Buffer::Instance &body, json json_body) {

  // remove content length, as we have new body.
  headers.removeContentLength();
  // we know that the new content type is json:
  headers.removeContentType();
  headers.setReferenceContentType(
      Http::Headers::get().ContentTypeValues.Json);

  // replace body
  body.drain(body.length());
  body.add(json_body.dump());
  headers.setContentLength(body.length());
}

BodyHeaderTransformer::BodyHeaderTransformer(bool add_request_metadata) : add_request_metadata_(add_request_metadata){}

BodyHeaderRequestTransformer::BodyHeaderRequestTransformer(bool add_request_metadata)
    : BodyHeaderTransformer(add_request_metadata){}

void BodyHeaderRequestTransformer::transform(
    Http::RequestHeaderMap &request_headers, Buffer::Instance &body,
    Http::StreamFilterCallbacks &) const {
  auto json_body = extractBodyAndHeaders(request_headers, body);
  if (add_request_metadata_) {
    const Http::HeaderString& path = request_headers.Path()->value();
    absl::string_view query_string = Http::Utility::findQueryStringStart(path);
    absl::string_view path_view = path.getStringView();
    path_view.remove_suffix(query_string.length());
    if (query_string.size() > 0) {
      // remove the question mark
      query_string.remove_prefix(1);
    }
    json_body["queryString"] = query_string;
    json_body["httpMethod"] = request_headers.Method()->value().getStringView();
    json_body["path"] = path_view;
  }

  mutate(request_headers, body, json_body);
}

BodyHeaderResponseTransformer::BodyHeaderResponseTransformer()
    : BodyHeaderTransformer(false){}

void BodyHeaderResponseTransformer::transform(
    Http::ResponseHeaderMap &response_headers,
    Buffer::Instance &body,
    Http::StreamFilterCallbacks &) const {
  auto json_body = extractBodyAndHeaders(response_headers, body);
  mutate(response_headers, body, json_body);
}

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
