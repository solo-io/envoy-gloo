#pragma once

#include <map>

#include "extensions/filters/http/transformation/transformer.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

class BodyHeaderTransformer : public Transformer {
public:
  void transform(Http::HeaderMap &map, Buffer::Instance &body) const override;
};

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
