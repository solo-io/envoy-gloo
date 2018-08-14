#include "common/http/filter/route_fault_filter.h"

namespace Envoy {
namespace Http {

bool RouteFaultFilter::shouldActivate(
    const ProtobufWkt::Struct &filter_metadata_struct) {

  const auto &fields = filter_metadata_struct.fields();
  const auto fields_it =
      fields.find(Config::MetadataRouteFaultKeys::get().FAULT_NAME);
  if (fields_it == fields.end()) {
    return false;
  }

  const ProtobufWkt::Value &route_name_value = fields_it->second;

  if (route_name_value.kind_case() != ProtobufWkt::Value::kStringValue) {
    return false;
  }

  return route_name_value.string_value() != route_filter_config_->name();
}

} // namespace Http
} // namespace Envoy
