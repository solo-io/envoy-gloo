#include "source/extensions/filters/http/transformation/transformer.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

namespace {
constexpr uint64_t MAX_STAGE_NUMBER = 10UL;
}

TransformerPair::TransformerPair(RequestTransformerConstSharedPtr request_transformer,
                                 ResponseTransformerConstSharedPtr response_transformer,
                                 OnStreamCompleteTransformerConstSharedPtr on_stream_completion_transformer,
                                 bool should_clear_cache)
    : clear_route_cache_(should_clear_cache),
      request_transformation_(request_transformer),
      response_transformation_(response_transformer),
      on_stream_completion_transformation_(on_stream_completion_transformer) {}

TransformerPairConstSharedPtr
FilterConfig::findTransformers(const Http::RequestHeaderMap &headers) const {
  for (const auto &pair : transformerPairs()) {
    if (pair.matcher()->matches(headers)) {
      return pair.transformer_pair();
    }
  }
  return nullptr;
}

TransformationFilterStats FilterConfig::generateStats(const std::string &prefix,
                                                      Stats::Scope &scope) {
  const std::string final_prefix = prefix + "transformation.";
  return {ALL_TRANSFORMATION_FILTER_STATS(
      POOL_COUNTER_PREFIX(scope, final_prefix))};
}

RouteFilterConfig::RouteFilterConfig() : stages_(MAX_STAGE_NUMBER + 1) {}

const TransformConfig *
RouteFilterConfig::transformConfigForStage(uint32_t stage) const {
  ASSERT(stage < stages_.size());
  return stages_[stage].get();
}

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
