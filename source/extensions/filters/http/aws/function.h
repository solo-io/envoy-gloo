#pragma once

#include <map>
#include <string>
#include <tuple>

#include "common/protobuf/utility.h"

#include "absl/types/optional.h"

namespace Envoy {
namespace Http {

struct Function {

  // TODO(yuval-k): Remove this when we have a optional that can support types
  // without a default ctor.
  Function()
      : name_(nullptr), qualifier_(nullptr), async_(), host_(nullptr),
        region_(nullptr), access_key_(nullptr), secret_key_(nullptr) {}

  Function(const std::string *name,
           absl::optional<const std::string *> qualifier, bool async,
           const std::string *host, const std::string *region,
           const std::string *access_key, const std::string *secret_key)
      : name_(name), qualifier_(qualifier), async_(async), host_(host),
        region_(region), access_key_(access_key), secret_key_(secret_key) {}

  static absl::optional<Function>
  createFunction(absl::optional<const std::string *> name,
                 absl::optional<const std::string *> qualifier, bool async,
                 absl::optional<const std::string *> host,
                 absl::optional<const std::string *> region,
                 absl::optional<const std::string *> access_key,
                 absl::optional<const std::string *> secret_key);

  const std::string *name_{nullptr};
  absl::optional<const std::string *> qualifier_;
  bool async_{false};

  const std::string *host_{nullptr};
  const std::string *region_{nullptr};
  const std::string *access_key_{nullptr};
  const std::string *secret_key_{nullptr};
};

} // namespace Http
} // namespace Envoy
