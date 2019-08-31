#include "extensions/filters/http/transformation/transformation_filter_config.h"

#include "extensions/filters/http/transformation/body_header_transformer.h"
#include "extensions/filters/http/transformation/inja_transformer.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

Transformation::Transformation(
    const envoy::api::v2::filter::http::Transformation &transformation)
    : transformation_(transformation) {
  switch (transformation_.transformation_type_case()) {
  case envoy::api::v2::filter::http::Transformation::kTransformationTemplate:
    passthrough_body_ =
        transformation_.transformation_template().has_passthrough();
    transformer_.reset(
        new InjaTransformer(transformation_.transformation_template()));
    break;
  case envoy::api::v2::filter::http::Transformation::kHeaderBodyTransform:
    transformer_.reset(new BodyHeaderTransformer());
    break;
  case envoy::api::v2::filter::http::Transformation::
      TRANSFORMATION_TYPE_NOT_SET:
  default:
    throw EnvoyException("non existant transformation");
  }
}

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
