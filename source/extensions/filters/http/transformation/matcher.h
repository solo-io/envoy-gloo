#pragma once

#include <string>

#include "source/common/http/matching/data_impl.h"
#include "envoy/matcher/matcher.h"
#include "source/extensions/filters/http/transformation/transformer.h"
#include "envoy/server/factory_context.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

TransformerPairConstSharedPtr matchTransform(Http::Matching::HttpMatchingDataImpl&& data, const Envoy::Matcher::MatchTreeSharedPtr<Http::HttpMatchingData>& matcher);

Envoy::Matcher::MatchTreeSharedPtr<Http::HttpMatchingData> createTransformationMatcher(
    const xds::type::matcher::v3::Matcher &matcher,
    Server::Configuration::ServerFactoryContext &factory_context);

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
