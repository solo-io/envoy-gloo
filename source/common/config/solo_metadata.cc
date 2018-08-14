#include "common/config/solo_metadata.h"

namespace Envoy {
namespace Config {

absl::optional<const std::string *>
SoloMetadata::nonEmptyStringValue(const ProtobufWkt::Struct &spec,
                                  const std::string &key) {

  absl::optional<const ProtobufWkt::Value *> maybe_value = value(spec, key);
  if (!maybe_value.has_value()) {
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

  return absl::optional<const std::string *>(&string_value);
}

bool SoloMetadata::boolValue(const ProtobufWkt::Struct &spec,
                             const std::string &key) {
  absl::optional<const ProtobufWkt::Value *> maybe_value = value(spec, key);
  if (!maybe_value.has_value()) {
    return {};
  }

  const auto &value = *maybe_value.value();
  if (value.kind_case() != ProtobufWkt::Value::kBoolValue) {
    return {};
  }

  return value.bool_value();
}

absl::optional<const ProtobufWkt::Value *>
SoloMetadata::value(const ProtobufWkt::Struct &spec, const std::string &key) {
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
