#pragma once

#include <map>
#include <string>
#include <tuple>

namespace Envoy {
namespace Http {

struct Function {
  std::string func_name_;
  std::string hostname_;
  std::string region_;
};

inline bool operator==(const Function &lhs, const Function &rhs) {
  return std::tie(lhs.func_name_, lhs.hostname_, lhs.region_) ==
         std::tie(rhs.func_name_, rhs.hostname_, rhs.region_);
}

using ClusterFunctionMap = std::map<std::string, Function>;

} // namespace Http
} // namespace Envoy
