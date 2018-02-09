#pragma once

#include <string>

#include "lambda_filter.pb.h"

namespace Envoy {
namespace Http {

class LambdaFilterConfig {

  using ProtoConfig = envoy::api::v2::filter::http::Lambda;

public:
  LambdaFilterConfig(const ProtoConfig &/*proto_config*/) {}
};

typedef std::shared_ptr<LambdaFilterConfig> LambdaFilterConfigSharedPtr;

} // namespace Http
} // namespace Envoy
