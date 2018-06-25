#pragma once

#include "common/singleton/const_singleton.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {

// TODO(talnordan): TODO: Merge with
// envoy/source/extensions/filters/network/well_known_names.h

/**
 * Well-known network filter names.
 */
class ConsulConnectNetworkFilterNameValues {
public:
  // Consul Connect filter
  const std::string CONSUL_CONNECT = "io.solo.filters.network.consul_connect";
};

typedef ConstSingleton<ConsulConnectNetworkFilterNameValues>
    ConsulConnectNetworkFilterNames;

} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
