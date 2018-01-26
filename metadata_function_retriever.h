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
  Optional<Function> getFunction(const ClusterInfo &info) override;
  Optional<Function> getFunction(const envoy::api::v2::Metadata &metadata);
  Optional<Function> getFunction(const FieldMap &fields);

  /**
   * TODO: Constants like these are typically declared in
   * envoy/source/common/config/well_known_names.h.
   */
  static const std::string ENVOY_LAMBDA;
  static const std::string FUNCTION_FUNC_NAME;
  static const std::string FUNCTION_HOSTNAME;
  static const std::string FUNCTION_REGION;

private:
  static inline Optional<const FieldMap *>
  filterMetadataFields(const envoy::api::v2::Metadata &metadata,
                       const std::string &filter);

  static inline Optional<const std::string *>
  nonEmptyStringValue(const FieldMap &fields, const std::string &key);
};

} // namespace Http
} // namespace Envoy
