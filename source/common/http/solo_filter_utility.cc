#include "source/common/http/solo_filter_utility.h"

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
    // TODO(talnordan): Consider making the return type `optional`.
    return nullptr;
  }

  return &route_entry->clusterName();
}

} // namespace Http
} // namespace Envoy
