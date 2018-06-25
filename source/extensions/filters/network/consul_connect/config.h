#pragma once

#include "api/envoy/config/filter/network/consul_connect/v2/consul_connect.pb.validate.h"
#include "extensions/filters/network/common/factory_base.h"
#include "extensions/filters/network/consul_connect_well_known_names.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace ConsulConnect {

/**
 * Config registration for the Consul Connect filter. @see
 * NamedNetworkFilterConfigFactory.
 */
class ConfigFactory
    : public Common::FactoryBase<
          envoy::config::filter::network::consul_connect::v2::ConsulConnect> {
public:
  ConfigFactory()
      : FactoryBase(ConsulConnectNetworkFilterNames::get().CONSUL_CONNECT) {}

private:
  Network::FilterFactoryCb createFilterFactoryFromProtoTyped(
      const envoy::config::filter::network::consul_connect::v2::ConsulConnect
          &proto_config,
      Server::Configuration::FactoryContext &context) override;
};

} // namespace ConsulConnect
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
