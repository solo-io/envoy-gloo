#include "metadata_function_retriever.h"

#include "common/config/metadata.h"
#include "common/config/well_known_names.h"

#include "lambda_filter.pb.h"

namespace Envoy {
namespace Http {

using Config::Metadata;

Optional<Function>
MetadataFunctionRetriever::getFunction(const ClusterInfo &info) {
  return getFunction(info.metadata());
}

Optional<Function> MetadataFunctionRetriever::getFunction(
    const envoy::api::v2::Metadata &metadata) {

  auto lambda_filter_metadata_fields =
      filterMetadataFields(metadata, ENVOY_LAMBDA);

  if (!lambda_filter_metadata_fields.valid()) {
    return {};
  }

  return getFunction(*lambda_filter_metadata_fields.value());
}

Optional<Function>
MetadataFunctionRetriever::getFunction(const FieldMap &fields) {
  auto func_name = stringValue(fields, FUNCTION_FUNC_NAME);
  if (!func_name.valid()) {
    return {};
  }

  auto hostname = stringValue(fields, FUNCTION_HOSTNAME);
  if (!hostname.valid()) {
    return {};
  }

  auto region = stringValue(fields, FUNCTION_REGION);
  if (!region.valid()) {
    return {};
  }

  return Function::create(*func_name.value(), *hostname.value(),
                          *region.value());
}

const std::string MetadataFunctionRetriever::ENVOY_LAMBDA = "envoy.lambda";

const std::string MetadataFunctionRetriever::FUNCTION_FUNC_NAME =
    "function.func_name";

const std::string MetadataFunctionRetriever::FUNCTION_HOSTNAME =
    "function.hostname";

const std::string MetadataFunctionRetriever::FUNCTION_REGION =
    "function.region";

/**
 * TODO: Consider moving the `Struct` extraction logic to `Metadata`:
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
 * TODO: Consider moving the this logic to `Metadata`:
 * envoy/source/common/config/metadata.cc
 */
Optional<const std::string *>
MetadataFunctionRetriever::stringValue(const FieldMap &fields,
                                       const std::string &key) {
  const auto fields_it = fields.find(key);
  if (fields_it == fields.end()) {
    return {};
  }
  const auto &string_value = fields_it->second.string_value();
  return Optional<const std::string *>(&string_value);
}

} // namespace Http
} // namespace Envoy
