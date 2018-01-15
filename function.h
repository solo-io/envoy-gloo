#pragma once

#include <map>
#include <string>

namespace Envoy {
namespace Http {

struct Function {
  std::string func_name_;
  std::string hostname_;
  std::string region_;
};

using ClusterFunctionMap = std::map<std::string, Function>;

} // namespace Http
} // namespace Envoy
