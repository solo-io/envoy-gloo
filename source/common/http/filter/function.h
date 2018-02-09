#pragma once

#include <map>
#include <string>
#include <tuple>

#include "envoy/common/optional.h"
#include "common/protobuf/utility.h"

namespace Envoy {
namespace Http {

struct Function {
  const std::string *name_{nullptr};
  const std::string *qualifier_{nullptr};
  bool  async_{false};

  const std::string *host_{nullptr};
  const std::string *region_{nullptr};
  const std::string *access_key_{nullptr};
  const std::string *secret_key_{nullptr};
};

} // namespace Http
} // namespace Envoy
