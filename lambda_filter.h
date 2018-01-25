#pragma once

#include <map>
#include <string>

#include "envoy/upstream/cluster_manager.h"

#include "common/common/logger.h"

#include "server/config/network/http_connection_manager.h"

#include "aws_authenticator.h"
#include "function.h"
#include "lambda_filter.pb.h"
#include "lambda_filter_config.h"
#include "map_function_retriever.h"

namespace Envoy {
namespace Http {

using Envoy::Upstream::ClusterManager;

class LambdaFilter : public StreamDecoderFilter,
                     public Logger::Loggable<Logger::Id::filter> {
public:
  LambdaFilter(LambdaFilterConfigSharedPtr, FunctionRetrieverSharedPtr,
               ClusterManager &);
  ~LambdaFilter();

  // Http::StreamFilterBase
  void onDestroy() override;

  // Http::StreamDecoderFilter
  FilterHeadersStatus decodeHeaders(HeaderMap &, bool) override;
  FilterDataStatus decodeData(Buffer::Instance &, bool) override;
  FilterTrailersStatus decodeTrailers(HeaderMap &) override;
  void setDecoderFilterCallbacks(StreamDecoderFilterCallbacks &) override;

private:
  const LambdaFilterConfigSharedPtr config_;
  FunctionRetrieverSharedPtr functionRetriever_;
  ClusterManager &cm_;

  StreamDecoderFilterCallbacks *decoder_callbacks_;

  const std::string awsAccess() const { return config_->awsAccess(); }
  const std::string awsSecret() const { return config_->awsSecret(); }

  Function currentFunction_;
  void lambdafy();
  std::string functionUrlPath();

  Envoy::Http::HeaderMap *request_headers_{};
  bool active_;
  AwsAuthenticator awsAuthenticator_;
};

} // namespace Http
} // namespace Envoy
