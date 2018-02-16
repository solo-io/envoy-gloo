#include "common/http/filter/metadata_function_retriever.h"

#include "common/common/macros.h"
#include "common/config/solo_lambda_well_known_names.h"

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

  Function f;

  // internal scope to prevent copy-paste  errors
  {
    Optional<const std::string *> host = nonEmptyStringValue(
        upstream_spec, Config::MetadataLambdaKeys::get().HOSTNAME);
    if (!host.valid()) {
      ;
      return {};
    }
    f.host_ = host.value();
  }
  {
    Optional<const std::string *> region = nonEmptyStringValue(
        upstream_spec, Config::MetadataLambdaKeys::get().REGION);
    if (!region.valid()) {
      return {};
    }
    f.region_ = region.value();
  }

  {
    Optional<const std::string *> access_key = nonEmptyStringValue(
        upstream_spec, Config::MetadataLambdaKeys::get().ACCESS_KEY);
    if (!access_key.valid()) {
      return {};
    }
    f.access_key_ = access_key.value();
  }
  {
    Optional<const std::string *> secret_key = nonEmptyStringValue(
        upstream_spec, Config::MetadataLambdaKeys::get().SECRET_KEY);
    if (!secret_key.valid()) {
      return {};
    }
    f.secret_key_ = secret_key.value();
  }
  {
    Optional<const std::string *> name = nonEmptyStringValue(
        function_spec, Config::MetadataLambdaKeys::get().FUNC_NAME);
    if (!name.valid()) {
      return {};
    }
    f.name_ = name.value();
  }

  f.qualifier_ = nonEmptyStringValue(
      function_spec, Config::MetadataLambdaKeys::get().FUNC_QUALIFIER);

  if (maybe_route_spec.valid()) {
    f.async_ = boolValue(*maybe_route_spec.value(),
                         Config::MetadataLambdaKeys::get().FUNC_ASYNC);
  }

  return f;
}

/**
 * TODO(talnordan): Consider moving the this logic to `Metadata`:
 * envoy/source/common/config/metadata.cc
 */
Optional<const std::string *>
MetadataFunctionRetriever::nonEmptyStringValue(const ProtobufWkt::Struct &spec,
                                               const std::string &key) const {

  const auto &fields = spec.fields();
  const auto fields_it = fields.find(key);
  if (fields_it == fields.end()) {
    return {};
  }

  const auto &value = fields_it->second;
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
                                          const std::string &key) const {
  const auto &fields = spec.fields();
  const auto fields_it = fields.find(key);
  if (fields_it == fields.end()) {
    return {};
  }

  const auto &value = fields_it->second;
  if (value.kind_case() != ProtobufWkt::Value::kBoolValue) {
    return {};
  }

  return value.bool_value();
}

} // namespace Http
} // namespace Envoy
