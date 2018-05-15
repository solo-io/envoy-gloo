#include "server/config/http/lambda_filter_config_factory.h"

#include "envoy/registry/registry.h"

#include "common/http/filter/lambda_filter.h"
#include "common/http/filter/lambda_filter_config.h"
#include "common/http/filter/metadata_function_retriever.h"
#include "common/http/functional_stream_decoder_base.h"

namespace Envoy {
namespace Server {
namespace Configuration {

typedef Http::FunctionalFilterMixin<Http::LambdaFilter> MixedLambdaFilter;

Http::FilterFactoryCb
LambdaFilterConfigFactory::createFilterFactoryFromProtoTyped(
    const envoy::api::v2::filter::http::Lambda &proto_config,
    const std::string &, FactoryContext &context) {

  Http::LambdaFilterConfigSharedPtr config =
      std::make_shared<Http::LambdaFilterConfig>(proto_config);

  Http::FunctionRetrieverSharedPtr functionRetriever =
      std::make_shared<Http::MetadataFunctionRetriever>();

  return [&context, config, functionRetriever](
             Http::FilterChainFactoryCallbacks &callbacks) -> void {
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
static Registry::RegisterFactory<LambdaFilterConfigFactory,
                                 NamedHttpFilterConfigFactory>
    register_;

} // namespace Configuration
} // namespace Server
} // namespace Envoy
