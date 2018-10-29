#include "extensions/filters/http/aws_lambda/config.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {

AWSLambdaRouteConfig::AWSLambdaRouteConfig(
    const envoy::config::filter::http::aws_lambda::v2::AWSLambdaPerRoute
        &protoconfig)
    : name_(protoconfig.name()), qualifier_(protoconfig.qualifier()),
      async_(protoconfig.async()) {

  if (protoconfig.has_empty_body_override()) {
    default_body_ = protoconfig.empty_body_override().value();
  }
}

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
