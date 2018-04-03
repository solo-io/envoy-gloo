#pragma once

#include <string>

#include "common/protobuf/protobuf.h"

#include "absl/types/optional.h"

namespace Envoy {
namespace Config {

/**
 * Config metadata helpers.
 *
 * TODO(talnordan): Merge with `Metadata`: envoy/source/common/config/metadata.h
 */
class SoloMetadata {
public:
  static absl::optional<const std::string *>
  nonEmptyStringValue(const Protobuf::Struct &spec, const std::string &key);

  static bool boolValue(const Protobuf::Struct &spec, const std::string &key);

  static absl::optional<const Protobuf::Value *>
  value(const Protobuf::Struct &spec, const std::string &key);
};

} // namespace Config
} // namespace Envoy
