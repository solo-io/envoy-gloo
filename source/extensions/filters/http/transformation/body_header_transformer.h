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

private:
  bool add_request_metadata_{};

};

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
