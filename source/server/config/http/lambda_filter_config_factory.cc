#include "server/config/http/lambda_filter_config_factory.h"

#include "envoy/registry/registry.h"

#include "common/http/filter/lambda_filter.h"
#include "common/http/filter/metadata_function_retriever.h"
#include "common/http/functional_stream_decoder_base.h"

namespace Envoy {
namespace Server {
namespace Configuration {

typedef Http::FunctionalFilterMixin<Http::LambdaFilter> MixedLambdaFilter;

Http::FilterFactoryCb
LambdaFilterConfigFactory::createFilter(const std::string &,
                                        FactoryContext &context) {

  Http::FunctionRetrieverSharedPtr functionRetriever =
      std::make_shared<Http::MetadataFunctionRetriever>();

  return [&context, functionRetriever](
             Http::FilterChainFactoryCallbacks &callbacks) -> void {
    auto filter = new MixedLambdaFilter(
        context, Config::LambdaMetadataFilters::get().LAMBDA,
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
