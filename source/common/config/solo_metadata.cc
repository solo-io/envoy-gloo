#include "common/config/solo_metadata.h"

namespace Envoy {
namespace Config {

Optional<const std::string *>
SoloMetadata::nonEmptyStringValue(const ProtobufWkt::Struct &spec,
                                  const std::string &key) {

  Optional<const Protobuf::Value *> maybe_value = value(spec, key);
  if (!maybe_value.valid()) {
    return {};
  }
  const auto &value = *maybe_value.value();
  if (value.kind_case() != ProtobufWkt::Value::kStringValue) {
    return {};
  }

  const auto &string_value = value.string_value();
  if (string_value.empty()) {
    return {};
  }

  return Optional<const std::string *>(&string_value);
}

bool SoloMetadata::boolValue(const Protobuf::Struct &spec,
                             const std::string &key) {
  Optional<const Protobuf::Value *> maybe_value = value(spec, key);
  if (!maybe_value.valid()) {
    return {};
  }

  const auto &value = *maybe_value.value();
  if (value.kind_case() != ProtobufWkt::Value::kBoolValue) {
    return {};
  }

  return value.bool_value();
}

Optional<const Protobuf::Value *>
SoloMetadata::value(const Protobuf::Struct &spec, const std::string &key) {
  const auto &fields = spec.fields();
  const auto fields_it = fields.find(key);
  if (fields_it == fields.end()) {
    return {};
  }

  const auto &value = fields_it->second;
  return &value;
}

} // namespace Config
} // namespace Envoy
