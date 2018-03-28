#pragma once

#include <string>

#include "envoy/common/optional.h"

#include "common/protobuf/protobuf.h"

namespace Envoy {
namespace Config {

/**
 * Config metadata helpers.
 *
 * TODO(talnordan): Merge with `Metadata`: envoy/source/common/config/metadata.h
 */
class SoloMetadata {
public:
  static Optional<const std::string *>
  nonEmptyStringValue(const Protobuf::Struct &spec, const std::string &key);

  static bool boolValue(const Protobuf::Struct &spec, const std::string &key);

  static Optional<const Protobuf::Value *> value(const Protobuf::Struct &spec,
                                                 const std::string &key);
};

} // namespace Config
} // namespace Envoy
