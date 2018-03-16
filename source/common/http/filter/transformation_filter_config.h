#pragma once

#include <string>

#include "transformation_filter.pb.h"

namespace Envoy {
namespace Http {

class TransformationFilterConfig {

  using ProtoConfig = envoy::api::v2::filter::http::Transformations;

public:
  TransformationFilterConfig(ProtoConfig proto_config);

  bool empty() const { return proto_config_.transformations().empty(); }
  bool advanced_templates() const { return proto_config_.advanced_templates(); }

  const envoy::api::v2::filter::http::Transformation *
  getTranformation(const std::string &name) const;

private:
  ProtoConfig proto_config_;
};

typedef std::shared_ptr<const TransformationFilterConfig>
    TransformationFilterConfigConstSharedPtr;

} // namespace Http
} // namespace Envoy
