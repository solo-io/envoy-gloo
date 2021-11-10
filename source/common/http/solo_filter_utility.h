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

    auto cluster = cluster_manager.getThreadLocalCluster(*cluster_name);

    if (!cluster) {
      return nullptr;
    }

    auto cluster_info = cluster->info();
    return cluster_info->extensionProtocolOptionsTyped<ConfigType>(filter_name);
  }

};

} // namespace Http
} // namespace Envoy
