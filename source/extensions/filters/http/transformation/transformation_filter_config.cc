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

class ResponseMatcherImpl : public ResponseMatcher {
public:
  ResponseMatcherImpl(const envoy::api::v2::filter::http::ResponseMatcher& match);
  bool matches(const Http::RequestHeaderMap& headers, const Http::StreamInfo &stream_info) const override;

private:
  std::vector<Http::HeaderUtility::HeaderDataPtr> headers_;
  absl::optional<StringMatcher> response_code_details_match_;
};

ResponseMatcherImpl::ResponseMatcherImpl(const envoy::api::v2::filter::http::ResponseMatcher& match) :
        headers_(Http::HeaderUtility::buildHeaderDataVector(match.headers())) {
  if (match.has_response_code_details()) {
    response_code_details_match_.emplace(match.response_code_details());
  }
}

bool ResponseMatcherImpl::matches(const Http::ResponseHeaderMap& headers, const Http::StreamInfo &stream_info) const {

  if (response_code_details_match_.has_value()) {
    const auto& maybe_details = stream_info.responseCodeDetails();
    if (!maybe_details.has_value()) {
      return false;
    }
    if (!response_code_details_match_.value().matches(maybe_details.value())) {
      return false;
    }
  }

  if (!Http::HeaderUtility::matchHeaders(headers, headers_)) {
    return false;
  }

  return true;
}

ResponseMatcherConstPtr ResponseMatcher::create(const envoy::api::v2::filter::http::ResponseMatcher& match) {
  return make_shared<const ResponseMatcherImpl>(match);
}

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
