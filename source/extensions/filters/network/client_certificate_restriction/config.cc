#include <string>

#include "envoy/registry/registry.h"
#include "envoy/server/filter_config.h"

#include "extensions/filters/network/client_certificate_restriction/client_certificate_restriction.h"

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
                               FactoryContext &context) override {
    return [&context](Network::FilterManager &filter_manager) -> void {
      filter_manager.addReadFilter(Network::ReadFilterSharedPtr{
          new Filter::ClientCertificateRestrictionFilter(
              context.clusterManager())});
    };
  }

  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return ProtobufTypes::MessagePtr{new Envoy::ProtobufWkt::Empty()};
  }

  std::string name() override {
    return "io.solo.client_certificate_restriction";
  }

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
