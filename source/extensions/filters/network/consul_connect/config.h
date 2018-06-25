#pragma once

#include "api/envoy/config/filter/network/client_certificate_restriction/v2/client_certificate_restriction.pb.validate.h"
#include "extensions/filters/network/client_certificate_restriction_well_known_names.h"
#include "extensions/filters/network/common/factory_base.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace ClientCertificateRestriction {

/**
 * Config registration for the client certificate restriction filter. @see
 * NamedNetworkFilterConfigFactory.
 */
class ConfigFactory
    : public Common::FactoryBase<
          envoy::config::filter::network::client_certificate_restriction::v2::
              ClientCertificateRestriction> {
public:
  ConfigFactory()
      : FactoryBase(ClientCertificateRestrictionNetworkFilterNames::get()
                        .CLIENT_CERTIFICATE_RESTRICTION) {}

private:
  Network::FilterFactoryCb createFilterFactoryFromProtoTyped(
      const envoy::config::filter::network::client_certificate_restriction::v2::
          ClientCertificateRestriction &proto_config,
      Server::Configuration::FactoryContext &context) override;
};

} // namespace ClientCertificateRestriction
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
