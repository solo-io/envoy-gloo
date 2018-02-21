#pragma once

#include <string>

#include "transformation_filter.pb.h"

namespace Envoy {
namespace Http {

class TransformationFilterConfig {

  using ProtoConfig = envoy::api::v2::filter::http::Transformations;

public:
  TransformationFilterConfig(ProtoConfig proto_config);
  
  const envoy::api::v2::filter::http::Transformation * getTranformation(const std::string & name);
private:
 ProtoConfig proto_config_;
};

typedef std::shared_ptr<TransformationFilterConfig>
    TransformationFilterConfigSharedPtr;

} // namespace Http
} // namespace Envoy
