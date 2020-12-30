#pragma once

#include "envoy/http/filter.h"
#include "envoy/upstream/cluster_manager.h"

namespace Envoy {
namespace Http {

/**
 * General utilities for HTTP filters.
 *
 * TODO(talnordan): Merge this class into
 * envoy/source/common.http/filter_utility.h.
 */
class SoloFilterUtility {
public:
  // TODO(talnordan): The envoyproxy/envoy convention seems to be not to
  // explicitly delete constructors.
  SoloFilterUtility() = delete;
  SoloFilterUtility(const SoloFilterUtility &) = delete;

  /**
   * Resolve the route entry.
   * @param decoder_callbacks supplies the decoder callback of filter.
   * @return the route entry selected for this request. Note: this will be
   * nullptr if no route was selected.
   */
  static const Router::RouteEntry *
  resolveRouteEntry(StreamFilterCallbacks *filter_callbacks);

  /**
   * Resolve the cluster name.
   * @param filter_callbacks supplies the encoder/decoder callback of filter.
   * @return the name of the cluster selected for this request. Note: this will
   * be nullptr if no route was selected.
   */
  static const std::string *
  resolveClusterName(StreamFilterCallbacks *filter_callbacks);

  /**
   * Retreives the route specific config. Route specific config can be in a few
   * places, that are checked in order. The first config found is returned. The
   * order is:
   * - the routeEntry() (for config that's applied on weighted clusters)
   * - the route
   * - and finally from the virtual host object (routeEntry()->virtualhost()).
   *
   * To use, simply:
   *
   *     auto route = stream_callbacks_.route();
   *     const auto* config =
   *         SoloFilterUtility::resolvePerFilterConfig(FILTER_NAME, route);
   *
   * See notes about config's lifetime below.
   *
   * @param filter_name The name of the filter who's route config should be
   * fetched.
   *
   * @param route The route to check for route configs. nullptr routes will
   * result in nullptr being reutrned.
   *
   * @return The route config if found. nullptr if not found. The returned
   * pointer's lifetime is the same as the route parameter.
   */
  template <class ConfigType>
  static const ConfigType *
  resolvePerFilterConfig(const std::string &filter_name,
                         const Router::RouteConstSharedPtr &route) {
    static_assert(
        std::is_base_of<Router::RouteSpecificFilterConfig, ConfigType>::value,
        "ConfigType must be a subclass of Router::RouteSpecificFilterConfig");
    return dynamic_cast<const ConfigType *>(
        resolvePerFilterBaseConfig(filter_name, route));
  }

  template <class ConfigType>
  static std::shared_ptr<const ConfigType>
  resolveProtocolOptions(const std::string &filter_name,
                         StreamFilterCallbacks *filter_callbacks,
                         Upstream::ClusterManager &cluster_manager) {
    static_assert(
        std::is_base_of<Upstream::ProtocolOptionsConfig, ConfigType>::value,
        "ConfigType must be a subclass of Upstream::ProtocolOptionsConfig");
    const auto *cluster_name = resolveClusterName(filter_callbacks);

    if (!cluster_name) {
      return nullptr;
    }

    auto cluster = cluster_manager.clusters().getCluster(*cluster_name);

    if (!cluster.has_value()) {
      return nullptr;
    }

    auto cluster_info = cluster->get().info();
    return cluster_info->extensionProtocolOptionsTyped<ConfigType>(filter_name);
  }

private:
  /**
   * The non template implementation of resolvePerFilterConfig. see
   * resolvePerFilterConfig for docs.
   */
  static const Router::RouteSpecificFilterConfig *
  resolvePerFilterBaseConfig(const std::string &filter_name,
                             const Router::RouteConstSharedPtr &route);
};

} // namespace Http
} // namespace Envoy
