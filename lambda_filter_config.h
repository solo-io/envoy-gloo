#pragma once

#include <string>

#include "lambda_filter.pb.h"

namespace Envoy {
namespace Http {

class LambdaFilterConfig {

  using ProtoConfig = envoy::api::v2::filter::http::Lambda;

public:
  LambdaFilterConfig(const ProtoConfig &proto_config);

  const std::string &aws_access() const { return aws_access_; }
  const std::string &aws_secret() const { return aws_secret_; }

private:
  const std::string aws_access_;
  const std::string aws_secret_;
};

typedef std::shared_ptr<LambdaFilterConfig> LambdaFilterConfigSharedPtr;

} // namespace Http
} // namespace Envoy
