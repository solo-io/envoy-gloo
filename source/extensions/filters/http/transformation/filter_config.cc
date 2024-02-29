#include "source/extensions/filters/http/transformation/filter_config.h"
#include "source/extensions/filters/http/transformation/matcher.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

namespace {
constexpr uint64_t MAX_STAGE_NUMBER = 10UL;
}

TransformerPairConstSharedPtr
FilterConfig::findTransformers(const Http::RequestHeaderMap &headers, StreamInfo::StreamInfo& si) const {
  auto match = matcher();
  if (match) {
      Http::Matching::HttpMatchingDataImpl data(si);
      data.onRequestHeaders(headers);
      return matchTransform(std::move(data), match);
  }
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
