#pragma once

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {

// TODO(talnordan): TODO: Merge with
// envoy/source/extensions/filters/network/well_known_names.h

/**
 * Well-known network filter names.
 */
class ClientCertificateRestrictionNetworkFilterNameValues {
public:
  // Client certificate restriction filter
  const std::string CLIENT_CERTIFICATE_RESTRICTION =
      "io.solo.filters.network.client_certificate_restriction";
};

typedef ConstSingleton<ClientCertificateRestrictionNetworkFilterNameValues>
    ClientCertificateRestrictionNetworkFilterNames;

} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
