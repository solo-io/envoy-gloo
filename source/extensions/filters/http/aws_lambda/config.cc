#include "extensions/filters/http/aws_lambda/config.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {

AWSLambdaRouteConfig::AWSLambdaRouteConfig(
    const envoy::config::filter::http::aws_lambda::v2::AWSLambdaPerRoute
        &protoconfig)
    : path_(functionUrlPath(protoconfig.name(), protoconfig.qualifier())),
      async_(protoconfig.async()) {

  if (protoconfig.has_empty_body_override()) {
    default_body_ = protoconfig.empty_body_override().value();
  }
}

std::string
AWSLambdaRouteConfig::functionUrlPath(const std::string &name,
                                      const std::string &qualifier) {

  std::stringstream val;
  val << "/2015-03-31/functions/" << name << "/invocations";
  if (!qualifier.empty()) {
    val << "?Qualifier=" << qualifier;
  }
  return val.str();
}

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
