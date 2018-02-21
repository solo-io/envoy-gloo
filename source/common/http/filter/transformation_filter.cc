#include "common/http/filter/transformation_filter.h"

namespace Envoy {
namespace Http {

TransformationFilter::TransformationFilter(TransformationFilterConfigSharedPtr config) : config_(config) {

}

TransformationFilter::~TransformationFilter() {

}

FilterHeadersStatus TransformationFilter::decodeHeaders(HeaderMap &, bool) {
    return FilterHeadersStatus::Continue;
}

FilterDataStatus TransformationFilter::decodeData(Buffer::Instance &, bool) {
    return FilterDataStatus::Continue;
}

FilterTrailersStatus TransformationFilter::decodeTrailers(HeaderMap &) {
    return FilterTrailersStatus::Continue;
}


} // namespace Http
} // namespace Envoy
