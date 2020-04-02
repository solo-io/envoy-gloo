#pragma once

#include <map>

#include "extensions/filters/http/transformation/transformer.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

class BodyHeaderTransformer : public Transformer {
public:
  void transform(Http::RequestOrResponseHeaderMap &map, Buffer::Instance &body,
    Http::StreamFilterCallbacks&) const override;
  bool passthrough_body() const override { return false; };
};

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
