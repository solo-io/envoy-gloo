#pragma once

#include <string>

#include "transformation_filter.pb.h"

namespace Envoy {
namespace Http {

class TransformationFilterConfig {

  using ProtoConfig = envoy::api::v2::filter::http::Transformations;

public:
  TransformationFilterConfig(const ProtoConfig &proto_config);
};

typedef std::shared_ptr<TransformationFilterConfig>
    TransformationFilterConfigSharedPtr;

} // namespace Http
} // namespace Envoy
