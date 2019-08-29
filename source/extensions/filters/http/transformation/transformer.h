#pragma once

#include "envoy/buffer/buffer.h"
#include "envoy/http/header_map.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

class Transformer {
public:
  virtual ~Transformer() {}

  virtual void transform(Http::HeaderMap &map,
                         Buffer::Instance &body) const PURE;
};

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
