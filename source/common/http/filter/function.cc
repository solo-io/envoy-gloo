#include "common/http/filter/function.h"

namespace Envoy {
namespace Http {

absl::optional<Function>
Function::createFunction(absl::optional<const std::string *> name,
                         absl::optional<const std::string *> qualifier,
                         bool async, absl::optional<const std::string *> host,
                         absl::optional<const std::string *> region,
                         absl::optional<const std::string *> access_key,
                         absl::optional<const std::string *> secret_key) {
  if (!name.has_value()) {
    return {};
  }
  if (!host.has_value()) {
    return {};
  }
  if (!region.has_value()) {
    return {};
  }
  if (!access_key.has_value()) {
    return {};
  }
  if (!secret_key.has_value()) {
    return {};
  }
  const Function f =
      Function(name.value(), qualifier, async, host.value(), region.value(),
               access_key.value(), secret_key.value());
  return absl::optional<Function>(f);
}

} // namespace Http
} // namespace Envoy
