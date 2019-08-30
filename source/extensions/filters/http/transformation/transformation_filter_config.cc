#include "extensions/filters/http/transformation/transformation_filter_config.h"

#include "extensions/filters/http/transformation/body_header_transformer.h"
#include "extensions/filters/http/transformation/inja_transformer.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

TransformerSharedPtr Transformation::getTransformer(
    const envoy::api::v2::filter::http::Transformation &transformation) {
  switch (transformation.transformation_type_case()) {
  case envoy::api::v2::filter::http::Transformation::kTransformationTemplate:
    return std::make_unique<InjaTransformer>(
        transformation.transformation_template());
  case envoy::api::v2::filter::http::Transformation::kHeaderBodyTransform:
    return std::make_unique<BodyHeaderTransformer>();
  case envoy::api::v2::filter::http::Transformation::
      TRANSFORMATION_TYPE_NOT_SET:
    // TODO: return null here?
  default:
    throw EnvoyException("non existant transformation");
  }
}

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
