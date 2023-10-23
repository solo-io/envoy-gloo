#include "api/envoy/config/filter/http/transformation/v2/transformation_filter.pb.validate.h"
#include "source/extensions/filters/http/transformation/transformation_factory.h"
#include "source/extensions/filters/http/transformation/body_header_transformer.h"
#include "source/extensions/filters/http/transformation/inja_transformer.h"
#include "source/common/config/utility.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

TransformerConstSharedPtr Transformation::getTransformer(
    const envoy::api::v2::filter::http::Transformation &transformation,
    Server::Configuration::CommonFactoryContext &context) {
  switch (transformation.transformation_type_case()) {
  case envoy::api::v2::filter::http::Transformation::kTransformationTemplate:
    return std::make_unique<InjaTransformer>(
        transformation.transformation_template(), context.api().randomGenerator(), context.threadLocal());
  case envoy::api::v2::filter::http::Transformation::kHeaderBodyTransform: {
    const auto& header_body_transform = transformation.header_body_transform();
    return std::make_unique<BodyHeaderTransformer>(header_body_transform.add_request_metadata());
  }
  case envoy::api::v2::filter::http::Transformation::kTransformerConfig: {
    auto &factory = Config::Utility::getAndCheckFactory<TransformerExtensionFactory>(transformation.transformer_config());
    auto config = Config::Utility::translateAnyToFactoryConfig(transformation.transformer_config().typed_config(), context.messageValidationContext().staticValidationVisitor(), factory);
    return factory.createTransformer(*co, context);
  }
  case envoy::api::v2::filter::http::Transformation::
      TRANSFORMATION_TYPE_NOT_SET:
    ENVOY_LOG(trace, "Request transformation type not set");
    FALLTHRU;
  default:
    throw EnvoyException("non existent transformation");
  }
}

std::unique_ptr<const TransformerPair> createTransformations(const envoy::api::v2::filter::http::TransformationRule_Transformations& route_transformation,
    Server::Configuration::CommonFactoryContext &context) {
    bool clear_route_cache = route_transformation.clear_route_cache();
    TransformerConstSharedPtr request_transformation;
    TransformerConstSharedPtr response_transformation;
    TransformerConstSharedPtr on_stream_completion_transformation;
    if (route_transformation.has_request_transformation()) {
      try {
        request_transformation = Transformation::getTransformer(
            route_transformation.request_transformation(), context);
      } catch (const std::exception &e) {
        throw EnvoyException(
            fmt::format("Failed to parse request template: {}", e.what()));
      }
    }
    if (route_transformation.has_response_transformation()) {
      try {
        response_transformation = Transformation::getTransformer(
            route_transformation.response_transformation(), context);
      } catch (const std::exception &e) {
        throw EnvoyException(
            fmt::format("Failed to parse response template: {}", e.what()));
      }
    }
    if (route_transformation.has_on_stream_completion_transformation()) {
      try {
        on_stream_completion_transformation = Transformation::getTransformer(
            route_transformation.on_stream_completion_transformation(),
            context);
      } catch (const std::exception &e) {
        throw EnvoyException(fmt::format(
            "Failed to get the on stream completion transformation: {}",
            e.what()));
      }
    }
    return std::make_unique<TransformerPair>(
          request_transformation, response_transformation,
          on_stream_completion_transformation, clear_route_cache);
}


}  // namespace Transformation
}  // namespace HttpFilters
}  // namespace Extensions
}  // namespace Envoy
