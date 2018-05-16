#pragma once

#include "envoy/router/router.h"

#include "cache_filter.pb.h"

namespace Envoy {
namespace Http {

class RouteSpecificCacheFilterConfig
    : public Router::RouteSpecificFilterConfig {
public:
  RouteSpecificCacheFilterConfig(
      const envoy::config::filter::http::cache::v2::CacheFilterRouteConfig
          &proto_config);

  bool isCacheable() const { return is_cacheable_; }

private:
  const bool is_cacheable_;
};

} // namespace Http
} // namespace Envoy
