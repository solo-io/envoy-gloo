#include "server/config/http/lambda_filter_config_factory.h"

#include <string>

#include "envoy/registry/registry.h"

#include "common/common/macros.h"
#include "common/config/json_utility.h"
#include "common/config/lambda_well_known_names.h"
#include "common/http/filter/function.h"
#include "common/http/filter/lambda_filter.h"
#include "common/http/filter/lambda_filter_config.h"
#include "common/http/filter/metadata_function_retriever.h"
#include "common/http/functional_stream_decoder_base.h"
#include "common/protobuf/utility.h"

#include "lambda_filter.pb.h"

namespace Envoy {
namespace Server {
namespace Configuration {

typedef Http::FunctionalFilterMixin<Http::LambdaFilter> MixedLambdaFilter;

HttpFilterFactoryCb LambdaFilterConfigFactory::createFilterFactory(
    const Json::Object &, const std::string &, FactoryContext &) {
  NOT_IMPLEMENTED;
}

HttpFilterFactoryCb LambdaFilterConfigFactory::createFilterFactoryFromProto(
    const Protobuf::Message &config, const std::string &stat_prefix,
    FactoryContext &context) {
  UNREFERENCED_PARAMETER(stat_prefix);

  /**
   * TODO:
   * The corresponding `.pb.validate.h` for the message is required by
   * Envoy::MessageUtil.
   * @see https://github.com/envoyproxy/envoy/pull/2194
   *
   * #include "lambda_filter.pb.validate.h"
   *
   * return createFilter(
   *    Envoy::MessageUtil::downcastAndValidate<const
   * envoy::api::v2::filter::http::Lambda&>(proto_config), context);
   * */

  return createFilter(
      dynamic_cast<const envoy::api::v2::filter::http::Lambda &>(config),
      context);
}

ProtobufTypes::MessagePtr LambdaFilterConfigFactory::createEmptyConfigProto() {
  return ProtobufTypes::MessagePtr{new envoy::api::v2::filter::http::Lambda()};
}

std::string LambdaFilterConfigFactory::name() {
  return Config::LambdaHttpFilterNames::get().LAMBDA;
}

HttpFilterFactoryCb LambdaFilterConfigFactory::createFilter(
    const envoy::api::v2::filter::http::Lambda &proto_config,
    FactoryContext &context) {

  Http::LambdaFilterConfigSharedPtr config =
      std::make_shared<Http::LambdaFilterConfig>(proto_config);

  Http::FunctionRetrieverSharedPtr functionRetriever =
      std::make_shared<Http::MetadataFunctionRetriever>();

  return [&context, config, functionRetriever](
             Envoy::Http::FilterChainFactoryCallbacks &callbacks) -> void {
    auto filter = new MixedLambdaFilter(
        context, Config::LambdaMetadataFilters::get().LAMBDA, config,
        functionRetriever);
    callbacks.addStreamDecoderFilter(
        Http::StreamDecoderFilterSharedPtr{filter});
  };
}

/**
 * Static registration for the AWS Lambda filter. @see RegisterFactory.
 */
static Envoy::Registry::RegisterFactory<
    LambdaFilterConfigFactory,
    Envoy::Server::Configuration::NamedHttpFilterConfigFactory>
    register_;

} // namespace Configuration
} // namespace Server
} // namespace Envoy
