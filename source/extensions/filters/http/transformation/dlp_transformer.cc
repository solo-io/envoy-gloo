#include "extensions/filters/http/transformation/dlp_transformer.h"

#include <iterator>

#include "common/common/macros.h"
#include "common/common/utility.h"
#include "common/http/headers.h"


namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {


DlpTransformer::DlpTransformer(
    const envoy::api::v2::filter::http::DlpTransformation &transformation) {

}
void DlpTransformer::transform(Http::HeaderMap &header_map,
                                      Buffer::Instance &body) const {

  // remove content length, as we have new body.
  header_map.removeContentLength();
  // we know that the new content type is json:
  header_map.removeContentType();
  header_map.insertContentType().value().setReference(
      Http::Headers::get().ContentTypeValues.Json);

  // replace body
  body.drain(body.length());
  body.add(json_body.dump());
  header_map.insertContentLength().value(body.length());
}

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
