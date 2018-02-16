#include "common/http/filter/function.h"

namespace Envoy {
namespace Http {

Optional<Function>
Function::createFunction(Optional<const std::string *> name,
                         Optional<const std::string *> qualifier, bool async,
                         Optional<const std::string *> host,
                         Optional<const std::string *> region,
                         Optional<const std::string *> access_key,
                         Optional<const std::string *> secret_key) {
  if (!name.valid()) {
    return {};
  }
  if (!region.valid()) {
    return {};
  }
  if (!host.valid()) {
    return {};
  }
  if (!access_key.valid()) {
    return {};
  }
  if (!secret_key.valid()) {
    return {};
  }
  const Function f =
      Function(name.value(), qualifier, async, host.value(), region.value(),
               access_key.value(), secret_key.value());
  return Optional<Function>(f);
}

} // namespace Http
} // namespace Envoy
