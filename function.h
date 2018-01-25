#pragma once

#include <map>
#include <string>
#include <tuple>

#include "envoy/common/optional.h"

namespace Envoy {
namespace Http {

struct Function {
  Function() : func_name_(""), hostname_(""), region_("") {}

  Function(const std::string &func_name, const std::string &hostname,
           const std::string &region)
      : func_name_(func_name), hostname_(hostname), region_(region) {}

  static Optional<Function> create(const std::string &func_name,
                                   const std::string &hostname,
                                   const std::string &region) {
    auto function = Function(func_name, hostname, region);
    if (function.valid()) {
      return Optional<Function>(function);
    }

    return {};
  }

  bool valid() const {
    return !(func_name_.empty() || hostname_.empty() || region_.empty());
  }

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
