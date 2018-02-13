#include "common/http/filter/metadata_function_retriever.h"

#include "common/common/macros.h"

namespace Envoy {
namespace Http {

MetadataFunctionRetriever::MetadataFunctionRetriever() {}

#define UPSTREAM_FIELDS(S, FIELD_FUNC)                                         \
  FIELD_FUNC(S, host)                                                          \
  FIELD_FUNC(S, region)                                                        \
  FIELD_FUNC(S, access_key)                                                    \
  FIELD_FUNC(S, secret_key)

#define FUNC_FIELDS(S, FIELD_FUNC)                                             \
  FIELD_FUNC(S, name)                                                          \
  FIELD_FUNC(S, qualifier)

#define ROUTE_FIELDS(S, FIELD_FUNC) FIELD_FUNC(S, async)

Optional<Function> MetadataFunctionRetriever::getFunctionFromSpec(
    const Protobuf::Struct &function_spec,
    const Protobuf::Struct &upstream_spec,
    const ProtobufWkt::Struct *route_spec) const {
  bool async = false;

#define CHECK(S, F)                                                            \
  {                                                                            \
    const auto &it = S.fields().find(#F);                                      \
    if (it == S.fields().end()) {                                              \
      return {};                                                               \
    }                                                                          \
    if (it->second.kind_case() != ProtobufWkt::Value::kStringValue) {          \
      return {};                                                               \
    }                                                                          \
  }

  FUNC_FIELDS(function_spec, CHECK);
  UPSTREAM_FIELDS(upstream_spec, CHECK);

  if (route_spec != nullptr) {
    // TODO: is this needed? ROUTE_FIELDS((*route_spec), CHECK);
    const auto &route_fields = route_spec->fields();
    async = (route_fields.at("async").bool_value());
  }

  // route spec contains function details,
  // filter spec contains upstream details.
  const auto &function_fields = function_spec.fields();
  const auto &upstream_fields = upstream_spec.fields();

#define GETSTRING(S, F) &(S.at(#F).string_value()),

  Function f = Function{FUNC_FIELDS(function_fields, GETSTRING) async,
                        UPSTREAM_FIELDS(upstream_fields, GETSTRING)};

  if (f.name_->empty()) {
    return {};
  }
  if (f.region_->empty()) {
    return {};
  }
  if (f.host_->empty()) {
    return {};
  }

  return f;
}

} // namespace Http
} // namespace Envoy
