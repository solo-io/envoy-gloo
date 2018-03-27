#include "common/http/filter/metadata_function_retriever.h"

#include "common/common/macros.h"
#include "common/config/lambda_well_known_names.h"

namespace Envoy {
namespace Http {

MetadataFunctionRetriever::MetadataFunctionRetriever() {}

Optional<Function> MetadataFunctionRetriever::getFunction(
    const MetadataAccessor &metadataccessor) const {

  Optional<const ProtobufWkt::Struct *> maybe_function_spec =
      metadataccessor.getFunctionSpec();
  Optional<const ProtobufWkt::Struct *> maybe_upstream_spec =
      metadataccessor.getClusterMetadata();
  Optional<const ProtobufWkt::Struct *> maybe_route_spec =
      metadataccessor.getRouteMetadata();

  if (!maybe_function_spec.valid()) {
    return {};
  }
  if (!maybe_upstream_spec.valid()) {
    return {};
  }
  const ProtobufWkt::Struct &function_spec = *maybe_function_spec.value();
  const ProtobufWkt::Struct &upstream_spec = *maybe_upstream_spec.value();

  Optional<const std::string *> host = nonEmptyStringValue(
      upstream_spec, Config::LambdaMetadataKeys::get().HOSTNAME);
  Optional<const std::string *> region = nonEmptyStringValue(
      upstream_spec, Config::LambdaMetadataKeys::get().REGION);
  Optional<const std::string *> access_key = nonEmptyStringValue(
      upstream_spec, Config::LambdaMetadataKeys::get().ACCESS_KEY);
  Optional<const std::string *> secret_key = nonEmptyStringValue(
      upstream_spec, Config::LambdaMetadataKeys::get().SECRET_KEY);
  Optional<const std::string *> name = nonEmptyStringValue(
      function_spec, Config::LambdaMetadataKeys::get().FUNC_NAME);
  Optional<const std::string *> qualifier = nonEmptyStringValue(
      function_spec, Config::LambdaMetadataKeys::get().FUNC_QUALIFIER);
  bool async = false;
  if (maybe_route_spec.valid()) {
    async = boolValue(*maybe_route_spec.value(),
                      Config::LambdaMetadataKeys::get().FUNC_ASYNC);
  }

  return Function::createFunction(name, qualifier, async, host, region,
                                  access_key, secret_key);
}

/**
 * TODO(talnordan): Consider moving the this logic to `Metadata`:
 * envoy/source/common/config/metadata.cc
 */
Optional<const std::string *>
MetadataFunctionRetriever::nonEmptyStringValue(const ProtobufWkt::Struct &spec,
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

bool MetadataFunctionRetriever::boolValue(const Protobuf::Struct &spec,
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
MetadataFunctionRetriever::value(const Protobuf::Struct &spec,
                                 const std::string &key) {
  const auto &fields = spec.fields();
  const auto fields_it = fields.find(key);
  if (fields_it == fields.end()) {
    return {};
  }

  const auto &value = fields_it->second;
  return &value;
}

} // namespace Http
} // namespace Envoy
