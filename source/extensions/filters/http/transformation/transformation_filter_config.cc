#include "source/extensions/filters/http/transformation/transformation_filter_config.h"

#include "source/common/common/assert.h"
#include "source/common/common/matchers.h"
#include "source/common/protobuf/message_validator_impl.h"
#include "source/common/protobuf/protobuf.h"
#include "source/extensions/filters/http/transformation/matcher.h"
#include "source/extensions/filters/http/transformation/transformation_factory.h"
#include "source/common/matcher/matcher.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

void TransformationFilterConfig::addTransformationLegacy(
    const envoy::api::v2::filter::http::TransformationRule &rule,
    Server::Configuration::ServerFactoryContext &context) {
  if (!rule.has_match()) {
    return;
  }
  TransformerConstSharedPtr request_transformation;
  TransformerConstSharedPtr response_transformation;
  TransformerConstSharedPtr on_stream_completion_transformation;
  bool clear_route_cache = false;

  TransformerPairConstSharedPtr transformer_pair = 
      std::make_unique<TransformerPair>(
          request_transformation, response_transformation,
          on_stream_completion_transformation, clear_route_cache);
  if (rule.has_route_transformations()) {
    transformer_pair = createTransformations(rule.route_transformations(), context);
  }
  transformer_pairs_.emplace_back(MatcherCopy::Matcher::create(rule.match(), context),
                                  transformer_pair);
}

TransformationFilterConfig::TransformationFilterConfig(
    const TransformationConfigProto &proto_config, const std::string &prefix,
    Server::Configuration::ServerFactoryContext &context)
    : FilterConfig(prefix, context.scope(), proto_config.stage(),
                   proto_config.log_request_response_info()) {
    if (proto_config.has_matcher()) {
      matcher_ = createTransformationMatcher(proto_config.matcher(), context);
      return;
    }
  for (const auto &rule : proto_config.transformations()) {
      addTransformationLegacy(rule, context);
  }
}

class ResponseMatcherImpl : public ResponseMatcher {
public:
  ResponseMatcherImpl(
      const envoy::api::v2::filter::http::ResponseMatcher &match,
      Server::Configuration::ServerFactoryContext& context)
    : headers_(Http::HeaderUtility::buildHeaderDataVector(match.headers(), context)) {
    if (match.has_response_code_details()) {
      response_code_details_match_.emplace(match.response_code_details(), context);
    }
  }
  bool matches(const Http::ResponseHeaderMap &headers,
               const StreamInfo::StreamInfo &stream_info) const override;

private:
  std::vector<Http::HeaderUtility::HeaderDataPtr> headers_;
  absl::optional<Matchers::StringMatcherImpl> response_code_details_match_;
};

bool ResponseMatcherImpl::matches(
    const Http::ResponseHeaderMap &headers,
    const StreamInfo::StreamInfo &stream_info) const {

  if (response_code_details_match_.has_value()) {
    const auto &maybe_details = stream_info.responseCodeDetails();
    if (!maybe_details.has_value()) {
      return false;
    }
    if (!response_code_details_match_.value().match(maybe_details.value())) {
      return false;
    }
  }

  if (!Http::HeaderUtility::matchHeaders(headers, headers_)) {
    return false;
  }

  return true;
}

ResponseMatcherConstPtr ResponseMatcher::create(
    const envoy::api::v2::filter::http::ResponseMatcher &match,
    Server::Configuration::ServerFactoryContext& context) {
  return std::make_unique<const ResponseMatcherImpl>(match, context);
}

RouteTransformationFilterConfig::RouteTransformationFilterConfig(
    RouteTransformationConfigProto proto_config,
    Server::Configuration::ServerFactoryContext &context) {

  if (proto_config.transformations_size() == 0) {
    // no new style config, convert the deprecated config:
    auto *transformation = proto_config.add_transformations();
    auto *request_match = transformation->mutable_request_match();
    if (proto_config.has_request_transformation()) {
      *request_match->mutable_request_transformation() =
          proto_config.request_transformation();
    }
    if (proto_config.has_response_transformation()) {
      *request_match->mutable_response_transformation() =
          proto_config.response_transformation();
    }
    request_match->set_clear_route_cache(proto_config.clear_route_cache());
  }

  std::vector<std::unique_ptr<PerStageRouteTransformationFilterConfig>>
      temp_stages(stages_.size());

  for (auto &&transformation : proto_config.transformations()) {
    RELEASE_ASSERT(transformation.stage() < stages_.size(), "");
    if (!temp_stages[transformation.stage()]) {
      temp_stages[transformation.stage()].reset(
          new PerStageRouteTransformationFilterConfig());
    }

    if (transformation.has_matcher()) {
      temp_stages[transformation.stage()]->setMatcher(createTransformationMatcher(transformation.matcher(), context));
    } else {
      temp_stages[transformation.stage()]->addTransformation(transformation, context);
    }
  }
  for (uint32_t i = 0; i < stages_.size(); i++) {
    stages_[i] = std::move(temp_stages[i]);
  }
}

void PerStageRouteTransformationFilterConfig::setMatcher(Envoy::Matcher::MatchTreeSharedPtr<Http::HttpMatchingData> matcher){
  if (matcher_ != nullptr){
    throw EnvoyException(fmt::format("Only one matcher is allowed per stage"));
  }
  matcher_ = matcher;
}

void PerStageRouteTransformationFilterConfig::addTransformation(
    const envoy::api::v2::filter::http::RouteTransformations_RouteTransformation
        &transformation, Server::Configuration::ServerFactoryContext &context) {
  using envoy::api::v2::filter::http::RouteTransformations_RouteTransformation;
  // create either request or response one.
  switch (transformation.match_case()) {
  case RouteTransformations_RouteTransformation::kRequestMatch: {
    auto &&request_match = transformation.request_match();
    TransformerConstSharedPtr request_transformation;
    TransformerConstSharedPtr response_transformation;
    MatcherCopy::MatcherConstPtr matcher;

    if (request_match.has_match()) {
      matcher = MatcherCopy::Matcher::create(request_match.match(), context);
    }

    bool clear_route_cache = request_match.clear_route_cache();
    if (request_match.has_request_transformation()) {
      try {
        request_transformation = Transformation::getTransformer(
            request_match.request_transformation(), context);
      } catch (const std::exception &e) {
        throw EnvoyException(
            fmt::format("Failed to parse request template: {}", e.what()));
      }
    }
    if (request_match.has_response_transformation()) {
      try {
        response_transformation = Transformation::getTransformer(
            request_match.response_transformation(), context);
      } catch (const std::exception &e) {
        throw EnvoyException(
            fmt::format("Failed to parse response template: {}", e.what()));
      }
    }

    if (request_transformation != nullptr ||
        response_transformation != nullptr) {
      TransformerPairConstSharedPtr transformer_pair =
          std::make_unique<TransformerPair>(request_transformation,
                                            response_transformation,
                                            nullptr,
                                            clear_route_cache);
      transformer_pairs_.emplace_back(matcher, transformer_pair);
    }
    break;
  }
  case RouteTransformations_RouteTransformation::kResponseMatch: {
    auto &&response_match = transformation.response_match();
    ResponseMatcherConstPtr matcher;
    if (response_match.has_match()) {
      matcher = ResponseMatcher::create(response_match.match(), context);
    }
    auto &&transformation = response_match.response_transformation();
    try {
      std::pair<ResponseMatcherConstPtr, TransformerConstSharedPtr> pair(
          std::move(matcher), Transformation::getTransformer(transformation, context));
      response_transformations_.emplace_back(std::move(pair));
    } catch (const std::exception &e) {
      throw EnvoyException(fmt::format(
          "Failed to parse response template on response matcher: {}",
          e.what()));
    }
    break;
  }
  case RouteTransformations_RouteTransformation::MATCH_NOT_SET: {
    // This should never happen due to validation
    ASSERT(false);
  }
  }
}

TransformerPairConstSharedPtr
PerStageRouteTransformationFilterConfig::findTransformers(
    const Http::RequestHeaderMap &headers, StreamInfo::StreamInfo& info) const {

  if (matcher_) {
      Http::Matching::HttpMatchingDataImpl data(info);
      data.onRequestHeaders(headers);
      return matchTransform(std::move(data), matcher_); 
  }

  for (const auto &pair : transformer_pairs_) {
    if (pair.matcher() == nullptr || pair.matcher()->matches(headers)) {
      return pair.transformer_pair();
    }
  }
  return nullptr;
}

TransformerConstSharedPtr
PerStageRouteTransformationFilterConfig::findResponseTransform(
    const Http::ResponseHeaderMap &headers, StreamInfo::StreamInfo &si) const {
  for (const auto &pair : response_transformations_) {
    if (pair.first == nullptr || pair.first->matches(headers, si)) {
      return pair.second;
    }
  }
  return nullptr;
}

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
