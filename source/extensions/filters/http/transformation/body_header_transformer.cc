#include "extensions/filters/http/transformation/body_header_transformer.h"

#include "common/http/headers.h"

#include "nlohmann/json.hpp"

// For convenience
using json = nlohmann::json;

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

void BodyHeaderTransformer::transform(
    Http::RequestOrResponseHeaderMap &header_map,
    const Http::RequestHeaderMap *, Buffer::Instance &body,
    Http::StreamFilterCallbacks &) const {
  json json_body;
  if (body.length() > 0) {
    json_body["body"] = body.toString();
  }

  json &headers = json_body["headers"];
  header_map.iterate(
      [](const Http::HeaderEntry &header,
         void *context) -> Http::HeaderMap::Iterate {
        json *headers_ptr = static_cast<json *>(context);
        json &headers = *headers_ptr;
        headers[std::string(header.key().getStringView())] =
            std::string(header.value().getStringView());
        return Http::HeaderMap::Iterate::Continue;
      },
      &headers);

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

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
