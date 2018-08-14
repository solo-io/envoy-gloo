#include "extensions/filters/http/transformation/body_header_transformer.h"

#include "common/http/headers.h"

// For convenience
using json = nlohmann::json;

namespace Envoy {
namespace Http {

void BodyHeaderTransformer::transform(HeaderMap &header_map,
                                      Buffer::Instance &body) {
  json json_body;
  // copied from base64.cc
  if (body.length() > 0) {
    std::string bodystring;
    bodystring.reserve(body.length());

    uint64_t num_slices = body.getRawSlices(nullptr, 0);
    Buffer::RawSlice slices[num_slices];
    body.getRawSlices(slices, num_slices);

    for (Buffer::RawSlice &slice : slices) {
      const char *slice_mem = static_cast<const char *>(slice.mem_);
      bodystring.append(slice_mem, slice.len_);
    }
    // parse the body as json
    json_body["body"] = bodystring;
  }

  json &headers = json_body["headers"];
  header_map.iterate(
      [](const HeaderEntry &header, void *context) -> HeaderMap::Iterate {
        json *headers_ptr = static_cast<json *>(context);
        json &headers = *headers_ptr;
        headers[header.key().c_str()] = header.value().c_str();
        return HeaderMap::Iterate::Continue;
      },
      &headers);

  // remove content length, as we have new body.
  header_map.removeContentLength();
  // we know that the new content type is json:
  header_map.removeContentType();
  header_map.insertContentType().value().setReference(
      Headers::get().ContentTypeValues.Json);

  // replace body
  body.drain(body.length());
  body.add(json_body.dump());
  header_map.insertContentLength().value(body.length());
}

} // namespace Http
} // namespace Envoy
