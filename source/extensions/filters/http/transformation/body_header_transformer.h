#pragma once

#include <map>

#include "envoy/buffer/buffer.h"
#include "envoy/http/header_map.h"

#include "json.hpp"

namespace Envoy {
namespace Http {

class BodyHeaderTransformer {
public:
  void transform(HeaderMap &map, Buffer::Instance &body);
};

} // namespace Http
} // namespace Envoy
