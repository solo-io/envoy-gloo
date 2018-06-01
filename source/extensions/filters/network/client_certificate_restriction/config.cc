#include <string>

#include "envoy/registry/registry.h"
#include "envoy/server/filter_config.h"

#include "client_certificate_restriction.h"

namespace Envoy {
namespace Server {
namespace Configuration {

/**
 * Config registration for the client certificate restriction filter. @see
 * NamedNetworkFilterConfigFactory.
 */
class ClientCertificateRestrictionConfigFactory
    : public NamedNetworkFilterConfigFactory {
public:
  Network::FilterFactoryCb
  createFilterFactoryFromProto(const Protobuf::Message &,
                               FactoryContext &) override {
    return [](Network::FilterManager &filter_manager) -> void {
      filter_manager.addReadFilter(Network::ReadFilterSharedPtr{
          new Filter::ClientCertificateRestrictionFilter()});
    };
  }

  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return ProtobufTypes::MessagePtr{new Envoy::ProtobufWkt::Empty()};
  }

  std::string name() override { return "client_certificate_restriction"; }

  Network::FilterFactoryCb createFilterFactory(const Json::Object &,
                                               FactoryContext &) override {
    NOT_IMPLEMENTED;
  }
};

/**
 * Static registration for the client certificate restriction filter. @see
 * RegisterFactory.
 */
static Registry::RegisterFactory<ClientCertificateRestrictionConfigFactory,
                                 NamedNetworkFilterConfigFactory>
    registered_;

} // namespace Configuration
} // namespace Server
} // namespace Envoy
