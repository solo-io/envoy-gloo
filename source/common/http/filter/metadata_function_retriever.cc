#include "common/http/filter/metadata_function_retriever.h"

#include "common/common/macros.h"

namespace Envoy {
namespace Http {

MetadataFunctionRetriever::MetadataFunctionRetriever() {}

#define CLUSTER_FIELDS(FIELD_FUNC)\
  FIELD_FUNC(host) \
  FIELD_FUNC(region) \
  FIELD_FUNC(access_key) \
  FIELD_FUNC(secret_key)

#define FUNC_FIELDS(FIELD_FUNC)\
  FIELD_FUNC(async) \
  FIELD_FUNC(name) \
  FIELD_FUNC(qualifier)

Optional<Function> MetadataFunctionRetriever::getFunction(const FunctionalFilterBase& filter) {
  const ProtobufWkt::Struct &function_spec = filter.getFunctionSpec();
  const ProtobufWkt::Struct &filter_spec = filter.getChildFilterSpec();

  #define CHECK(F) \
    if (function_spec.fields().find( #F ) == function_spec.fields().end()) { \
      return {}; \
    }
  FUNC_FIELDS(CHECK);
  #undef CHECK

  #define CHECK(F) \
    if (filter_spec.fields().find( #F ) == filter_spec.fields().end()) { \
      return {}; \
    }
  CLUSTER_FIELDS(CHECK);

  // function spec contains function details,
  // filter spec contains upstream details.
  const auto& function_fields = function_spec.fields();
  const auto& filter_fields = filter_spec.fields();


  #define GETSTRING(FIELD, F) &(FIELD.at( #F ).string_value()),
  #define GETBOOL(FIELD, F) (FIELD.at( #F ).bool_value()),

  return Function {
    GETSTRING(function_fields, name_)
    GETSTRING(function_fields, qualifier_)
    GETBOOL(function_fields, async_)
    GETSTRING(filter_fields, host_)
    GETSTRING(filter_fields, region_)
    GETSTRING(filter_fields, access_key_)
    GETSTRING(filter_fields, secret_key_)
  };
}

} // namespace Http
} // namespace Envoy
