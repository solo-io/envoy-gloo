#pragma once

#include <map>
#include <string>

#include "envoy/http/filter.h"
#include "envoy/upstream/cluster_manager.h"

#include "absl/types/optional.h"
#include "api/envoy/config/filter/http/aws_lambda/v2/aws_lambda.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {

class AWSLambdaRouteConfig : public Router::RouteSpecificFilterConfig {
public:
  AWSLambdaRouteConfig(
      const envoy::config::filter::http::aws_lambda::v2::AWSLambdaPerRoute
          &protoconfig);

  const std::string &name() const { return name_; }
  const std::string &qualifier() const { return qualifier_; }
  bool async() const { return async_; }
  const absl::optional<std::string> &defaultBody() const {
    return default_body_;
  }

private:
  std::string name_;
  std::string qualifier_;
  bool async_;
  absl::optional<std::string> default_body_;
};

class AWSLambdaProtocolExtensionConfig
    : public Upstream::ProtocolOptionsConfig {
public:
  AWSLambdaProtocolExtensionConfig(
      const envoy::config::filter::http::aws_lambda::v2::
          AWSLambdaProtocolExtension &protoconfig)
      : host_(protoconfig.host()), region_(protoconfig.region()),
        access_key_(protoconfig.access_key()),
        secret_key_(protoconfig.secret_key()) {}

  const std::string &host() const { return host_; }
  const std::string &region() const { return region_; }
  const std::string &access_key() const { return access_key_; }
  const std::string &secret_key() const { return secret_key_; }

private:
  std::string host_;
  std::string region_;
  std::string access_key_;
  std::string secret_key_;
};

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
