#include "server/config/http/transformation_filter_config_factory.h"

#include <string>

#include "envoy/registry/registry.h"

#include "common/common/macros.h"
#include "common/config/json_utility.h"
#include "common/config/transformation_well_known_names.h"
#include "common/http/filter/transformation_filter.h"
#include "common/http/filter/transformation_filter_config.h"
#include "common/protobuf/utility.h"
#include "common/http/functional_stream_decoder_base.h"

#include "transformation_filter.pb.h"

namespace Envoy {
namespace Server {
namespace Configuration {

typedef Http::FunctionalFilterMixin<Http::TransformationFilter> MixedTransformationFilter;

HttpFilterFactoryCb TransformationFilterConfigFactory::createFilterFactory(
    const Json::Object &, const std::string &, FactoryContext &) {
  NOT_IMPLEMENTED;
}

HttpFilterFactoryCb
TransformationFilterConfigFactory::createFilterFactoryFromProto(
    const Protobuf::Message &config, const std::string &stat_prefix,
    FactoryContext &context) {
  UNREFERENCED_PARAMETER(stat_prefix);

  /**
   * TODO:
   * The corresponding `.pb.validate.h` for the message is required by
   * Envoy::MessageUtil.
   * @see https://github.com/envoyproxy/envoy/pull/2194
   *
   * #include "transformation_filter.pb.validate.h"
   *
   * return createFilter(
   *    Envoy::MessageUtil::downcastAndValidate<const
   * envoy::api::v2::filter::http::Transformations&>(proto_config), context);
   * */

  return createFilter(
      dynamic_cast<const envoy::api::v2::filter::http::Transformations &>(
          config),
      context);
}

ProtobufTypes::MessagePtr
TransformationFilterConfigFactory::createEmptyConfigProto() {
  return ProtobufTypes::MessagePtr{
      new envoy::api::v2::filter::http::Transformations()};
}

std::string TransformationFilterConfigFactory::name() {
  return Config::TransformationFilterNames::get().TRANSFORMATION;
}

HttpFilterFactoryCb TransformationFilterConfigFactory::createFilter(
    const envoy::api::v2::filter::http::Transformations &proto_config,
    FactoryContext &context) {

  Http::TransformationFilterConfigSharedPtr config =
      std::make_shared<Http::TransformationFilterConfig>(proto_config);

  return [&context,
          config](Envoy::Http::FilterChainFactoryCallbacks &callbacks) -> void {
    if (!config->empty()) {
      auto filter = new Http::TransformationFilter(config);
      callbacks.addStreamFilter(Http::StreamFilterSharedPtr{filter});
      auto func_filter = new MixedTransformationFilter(context, Config::TransformationFilterNames::get().TRANSFORMATION, config);
      callbacks.addStreamFilter(Http::StreamFilterSharedPtr{func_filter});
    }
  };
}

/**
 * Static registration for this sample filter. @see RegisterFactory.
 */
static Envoy::Registry::RegisterFactory<
    TransformationFilterConfigFactory,
    Envoy::Server::Configuration::NamedHttpFilterConfigFactory>
    register_;

} // namespace Configuration
} // namespace Server
} // namespace Envoy
