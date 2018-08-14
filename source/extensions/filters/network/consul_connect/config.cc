#include "extensions/filters/network/consul_connect/config.h"

#include "envoy/registry/registry.h"

#include "extensions/filters/network/consul_connect/consul_connect.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace ConsulConnect {

Network::FilterFactoryCb ConfigFactory::createFilterFactoryFromProtoTyped(
    const envoy::config::filter::network::consul_connect::v2::ConsulConnect
        &proto_config,
    Server::Configuration::FactoryContext &context) {
  ASSERT(!proto_config.target().empty());

  ConfigSharedPtr filter_config(new Config(proto_config, context.scope()));
  return [&context,
          filter_config](Network::FilterManager &filter_manager) -> void {
    filter_manager.addReadFilter(
        std::make_shared<Filter>(filter_config, context.clusterManager()));
  };
}

/**
 * Static registration for the Consul Connect filter. @see
 * RegisterFactory.
 */
static Registry::RegisterFactory<
    ConfigFactory, Server::Configuration::NamedNetworkFilterConfigFactory>
    registered_;

} // namespace ConsulConnect
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
