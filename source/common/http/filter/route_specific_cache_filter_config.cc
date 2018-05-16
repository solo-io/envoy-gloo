#include "common/http/filter/route_specific_cache_filter_config.h"

namespace Envoy {
namespace Http {

RouteSpecificCacheFilterConfig::RouteSpecificCacheFilterConfig(
    const envoy::config::filter::http::cache::v2::CacheFilterRouteConfig
        &proto_config)
    : is_cacheable_(proto_config.is_cacheable()) {}

} // namespace Http
} // namespace Envoy
