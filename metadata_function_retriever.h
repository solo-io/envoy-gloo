#pragma once

#include <memory>
#include <string>

#include "envoy/upstream/upstream.h"

#include "common/protobuf/protobuf.h"

#include "function.h"
#include "function_retriever.h"

namespace Envoy {
namespace Http {

class MetadataFunctionRetriever : public FunctionRetriever {

  using FieldMap = Protobuf::Map<std::string, Protobuf::Value>;

public:
  MetadataFunctionRetriever(const std::string &filter_key,
                            const std::string &function_name_key,
                            const std::string &hostname_key,
                            const std::string &region_key);

  Optional<Function> getFunction(const RouteEntry &routeEntry,
                                 const ClusterInfo &info) override;
  Optional<Function> getFunction(const FieldMap &route_metadata_fields,
                                 const FieldMap &cluster_metadata_fields);

private:
  /**
   * Resolve the filter metadata fields.
   * @param filter_name an entity that has metadata.
   * @param filter_name the reverse DNS filter name.
   */
  template <typename T>
  static inline Optional<const FieldMap *>
  filterMetadataFields(const T &entity, const std::string &filter_name);

  static inline Optional<const FieldMap *>
  filterMetadataFields(const envoy::api::v2::Metadata &metadata,
                       const std::string &filter_name);

  static inline Optional<const std::string *>
  nonEmptyStringValue(const FieldMap &fields, const std::string &key);

  const std::string &filter_key_;
  const std::string &function_name_key_;
  const std::string &hostname_key_;
  const std::string &region_key_;
};

template <typename T>
Optional<const MetadataFunctionRetriever::FieldMap *>
MetadataFunctionRetriever::filterMetadataFields(
    const T &entity, const std::string &filter_name) {
  return filterMetadataFields(entity.metadata(), filter_name);
}

} // namespace Http
} // namespace Envoy
