#pragma once

#include <map>
#include <string>
#include <tuple>

#include "envoy/common/optional.h"
#include "common/protobuf/utility.h"

namespace Envoy {
namespace Http {

struct Function {
  const std::string *func_name_{nullptr};
  const std::string *func_qualifier_{nullptr};
  const std::string *hostname_{nullptr};
  const std::string *region_{nullptr};
  bool async_{false};

  static Optional<Function>  getFunction(const ProtobufWkt::Struct& /*funcspec*/, const ProtobufWkt::Struct& /*clusterspec*/) {

    return {};
      
  }

};

} // namespace Http
} // namespace Envoy
