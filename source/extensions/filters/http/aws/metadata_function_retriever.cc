#include "common/http/filter/metadata_function_retriever.h"

#include "common/common/macros.h"
#include "common/config/lambda_well_known_names.h"
#include "common/config/solo_metadata.h"

namespace Envoy {
namespace Http {

using Config::SoloMetadata;

MetadataFunctionRetriever::MetadataFunctionRetriever() {}

absl::optional<Function> MetadataFunctionRetriever::getFunction(
    const MetadataAccessor &metadataccessor) const {

  absl::optional<const ProtobufWkt::Struct *> maybe_function_spec =
      metadataccessor.getFunctionSpec();
  absl::optional<const ProtobufWkt::Struct *> maybe_upstream_spec =
      metadataccessor.getClusterMetadata();
  absl::optional<const ProtobufWkt::Struct *> maybe_route_spec =
      metadataccessor.getRouteMetadata();

  if (!maybe_function_spec.has_value()) {
    return {};
  }
  if (!maybe_upstream_spec.has_value()) {
    return {};
  }
  const ProtobufWkt::Struct &function_spec = *maybe_function_spec.value();
  const ProtobufWkt::Struct &upstream_spec = *maybe_upstream_spec.value();

  absl::optional<const std::string *> host = SoloMetadata::nonEmptyStringValue(
      upstream_spec, Config::LambdaMetadataKeys::get().HOSTNAME);
  absl::optional<const std::string *> region =
      SoloMetadata::nonEmptyStringValue(
          upstream_spec, Config::LambdaMetadataKeys::get().REGION);
  absl::optional<const std::string *> access_key =
      SoloMetadata::nonEmptyStringValue(
          upstream_spec, Config::LambdaMetadataKeys::get().ACCESS_KEY);
  absl::optional<const std::string *> secret_key =
      SoloMetadata::nonEmptyStringValue(
          upstream_spec, Config::LambdaMetadataKeys::get().SECRET_KEY);
  absl::optional<const std::string *> name = SoloMetadata::nonEmptyStringValue(
      function_spec, Config::LambdaMetadataKeys::get().FUNC_NAME);
  absl::optional<const std::string *> qualifier =
      SoloMetadata::nonEmptyStringValue(
          function_spec, Config::LambdaMetadataKeys::get().FUNC_QUALIFIER);
  bool async = false;
  if (maybe_route_spec.has_value()) {
    async =
        SoloMetadata::boolValue(*maybe_route_spec.value(),
                                Config::LambdaMetadataKeys::get().FUNC_ASYNC);
  }

  return Function::createFunction(name, qualifier, async, host, region,
                                  access_key, secret_key);
}

} // namespace Http
} // namespace Envoy
