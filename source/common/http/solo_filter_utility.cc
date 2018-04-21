#include "common/http/solo_filter_utility.h"

namespace Envoy {
namespace Http {

const Router::RouteEntry *
SoloFilterUtility::resolveRouteEntry(StreamFilterCallbacks *filter_callbacks) {
  Router::RouteConstSharedPtr route = filter_callbacks->route();
  if (!route) {
    return nullptr;
  }

  return route->routeEntry();
}

const std::string *
SoloFilterUtility::resolveClusterName(StreamFilterCallbacks *filter_callbacks) {
  const Router::RouteEntry *route_entry = resolveRouteEntry(filter_callbacks);
  if (!route_entry) {
    return nullptr;
  }

  return &route_entry->clusterName();
}

const Router::RouteSpecificFilterConfig *
SoloFilterUtility::resolvePerFilterBaseConfig(
    const std::string &filter_name, const Router::RouteConstSharedPtr &route) {
  if (!route) {
    return nullptr;
  }

  const Router::RouteSpecificFilterConfig *maybe_filter_config{};

  const Router::RouteEntry *routeEntry = route->routeEntry();
  if (routeEntry) {
    maybe_filter_config = routeEntry->perFilterConfig(filter_name);
  }

  if (!maybe_filter_config) {
    maybe_filter_config = route->perFilterConfig(filter_name);
  }

  if (!maybe_filter_config && routeEntry) {
    maybe_filter_config =
        routeEntry->virtualHost().perFilterConfig(filter_name);
  }
  return maybe_filter_config;
}

} // namespace Http
} // namespace Envoy
