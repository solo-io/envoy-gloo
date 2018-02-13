#pragma once

#include <map>
#include <string>

#include "envoy/upstream/cluster_manager.h"

#include "common/http/filter/aws_authenticator.h"
#include "common/http/filter/function.h"
#include "common/http/filter/lambda_filter_config.h"
#include "common/http/filter/function_retriever.h"
#include "common/http/functional_stream_decoder_base.h"

#include "server/config/network/http_connection_manager.h"

#include "lambda_filter.pb.h"

namespace Envoy {
namespace Http {

class LambdaFilter : public FunctionalFilterBase {
public:
  LambdaFilter(Http::FunctionRetrieverSharedPtr retreiver, Server::Configuration::FactoryContext &ctx,
               const std::string &name, LambdaFilterConfigSharedPtr config);
  ~LambdaFilter();

  // Http::StreamFilterBase
  void onDestroy() override;

  // Http::FunctionalFilterBase
  FilterHeadersStatus functionDecodeHeaders(HeaderMap &, bool) override;
  FilterDataStatus functionDecodeData(Buffer::Instance &, bool) override;
  FilterTrailersStatus functionDecodeTrailers(HeaderMap &) override;

private:
  const LambdaFilterConfigSharedPtr config_;
  FunctionRetrieverSharedPtr functionRetriever_;
  Upstream::ClusterManager &cm_;

  StreamDecoderFilterCallbacks *decoder_callbacks_;

  Function currentFunction_;
  void lambdafy();
  std::string functionUrlPath();

  Envoy::Http::HeaderMap *request_headers_{};
  union{
    AwsAuthenticator aws_authenticator_;
  };

};

} // namespace Http
} // namespace Envoy
