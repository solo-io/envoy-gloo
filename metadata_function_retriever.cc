#include "metadata_function_retriever.h"

#include "common/common/macros.h"
#include "common/config/metadata.h"

#include "lambda_filter.pb.h"

namespace Envoy {
namespace Http {

using Config::Metadata;

MetadataFunctionRetriever::MetadataFunctionRetriever(
    const std::string &filter_key, const std::string &function_name_key,
    const std::string &hostname_key, const std::string &region_key)
    : filter_key_(filter_key), function_name_key_(function_name_key),
      hostname_key_(hostname_key), region_key_(region_key) {}

Optional<Function>
MetadataFunctionRetriever::getFunction(const RouteEntry &routeEntry,
                                       const ClusterInfo &info) {
  // TODO(talnordan): Use routeEntry.metadata().
  UNREFERENCED_PARAMETER(routeEntry);

  return getFunction(info.metadata());
}

Optional<Function> MetadataFunctionRetriever::getFunction(
    const envoy::api::v2::Metadata &metadata) {

  auto lambda_filter_metadata_fields =
      filterMetadataFields(metadata, filter_key_);

  if (!lambda_filter_metadata_fields.valid()) {
    return {};
  }

  return getFunction(*lambda_filter_metadata_fields.value());
}

Optional<Function>
MetadataFunctionRetriever::getFunction(const FieldMap &fields) {
  auto func_name = nonEmptyStringValue(fields, function_name_key_);
  if (!func_name.valid()) {
    return {};
  }

  auto hostname = nonEmptyStringValue(fields, hostname_key_);
  if (!hostname.valid()) {
    return {};
  }

  auto region = nonEmptyStringValue(fields, region_key_);
  if (!region.valid()) {
    return {};
  }

  return Function{*func_name.value(), *hostname.value(), *region.value()};
}

/**
 * TODO(talnordan): Consider moving the `Struct` extraction logic to `Metadata`:
 * envoy/source/common/config/metadata.cc
 */
Optional<const MetadataFunctionRetriever::FieldMap *>
MetadataFunctionRetriever::filterMetadataFields(
    const envoy::api::v2::Metadata &metadata, const std::string &filter) {
  const auto filter_it = metadata.filter_metadata().find(filter);
  if (filter_it == metadata.filter_metadata().end()) {
    return {};
  }

  const auto &filter_metadata_struct = filter_it->second;
  const auto &filter_metadata_fields = filter_metadata_struct.fields();
  return Optional<const FieldMap *>(&filter_metadata_fields);
}

/**
 * TODO(talnordan): Consider moving the this logic to `Metadata`:
 * envoy/source/common/config/metadata.cc
 */
Optional<const std::string *>
MetadataFunctionRetriever::nonEmptyStringValue(const FieldMap &fields,
                                               const std::string &key) {
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

} // namespace Http
} // namespace Envoy
