#include "common/http/filter/transformation_filter_config.h"

namespace Envoy {
namespace Http {

TransformationFilterConfig::TransformationFilterConfig(ProtoConfig proto_config)
    : proto_config_(proto_config) {}

const envoy::api::v2::filter::http::Transformation *
TransformationFilterConfig::getTranformation(const std::string &name) const {

  const auto &transformations = proto_config_.transformations();

  const auto it = transformations.find(name);
  if (it == transformations.end()) {
    return nullptr;
  }
  return &it->second;
}

} // namespace Http
} // namespace Envoy
