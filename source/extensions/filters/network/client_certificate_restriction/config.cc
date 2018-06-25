#include "extensions/filters/network/client_certificate_restriction/config.h"

#include "envoy/registry/registry.h"

#include "extensions/filters/network/client_certificate_restriction/client_certificate_restriction.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace ClientCertificateRestriction {

Network::FilterFactoryCb ConfigFactory::createFilterFactoryFromProtoTyped(
    const envoy::config::filter::network::client_certificate_restriction::v2::
        ClientCertificateRestriction &proto_config,
    Server::Configuration::FactoryContext &context) {
  ASSERT(!proto_config.target().empty());

  ConfigSharedPtr filter_config(new Config(proto_config));
  return [&context,
          filter_config](Network::FilterManager &filter_manager) -> void {
    filter_manager.addReadFilter(
        std::make_shared<Filter>(filter_config, context.clusterManager()));
  };
}

/**
 * Static registration for the client certificate restriction filter. @see
 * RegisterFactory.
 */
static Registry::RegisterFactory<
    ConfigFactory, Server::Configuration::NamedNetworkFilterConfigFactory>
    registered_;

} // namespace ClientCertificateRestriction
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
