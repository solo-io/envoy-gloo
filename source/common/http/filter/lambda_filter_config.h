#pragma once

#include <string>

#include "lambda_filter.pb.h"

namespace Envoy {
namespace Http {

class LambdaFilterConfig {

  using ProtoConfig = envoy::api::v2::filter::http::Lambda;

public:
  LambdaFilterConfig(const ProtoConfig &proto_config)
      : aws_access_(proto_config.access_key()),
        aws_secret_(proto_config.secret_key()) {}

  const std::string &awsAccess() const { return aws_access_; }
  const std::string &awsSecret() const { return aws_secret_; }

private:
  const std::string aws_access_;
  const std::string aws_secret_;
};

typedef std::shared_ptr<LambdaFilterConfig> LambdaFilterConfigSharedPtr;

} // namespace Http
} // namespace Envoy
