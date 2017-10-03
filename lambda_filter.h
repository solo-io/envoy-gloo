#pragma once

#include <string>
#include <map>

#include "server/config/network/http_connection_manager.h"
#include "envoy/upstream/cluster_manager.h"

#include "common/common/logger.h"

#include "aws_authenticator.h"

namespace Solo {
namespace Lambda {

struct Function {
  std::string func_name_;
  std::string hostname_;
  std::string region_;
};

typedef std::map<std::string, Function> ClusterFunctionMap;


class LambdaFilter : public StreamDecoderFilter,  public Logger::Loggable<Logger::Id::filter> {
public:
  LambdaFilter(std::string access_key, std::string secret_key, ClusterFunctionMap functions);
  ~LambdaFilter();

  // Http::StreamFilterBase
  void onDestroy() override;

  // Http::StreamDecoderFilter
  FilterHeadersStatus decodeHeaders(HeaderMap& headers, bool) override;
  FilterDataStatus decodeData(Buffer::Instance&, bool) override;
  FilterTrailersStatus decodeTrailers(HeaderMap&) override;
  void setDecoderFilterCallbacks(StreamDecoderFilterCallbacks& callbacks) override;

private:
  StreamDecoderFilterCallbacks* decoder_callbacks_;
  ClusterFunctionMap functions_;
  Function currentFunction_;
  void lambdafy();
  std::string functionUrlPath();

  Http::HeaderMap* request_headers_{};
  bool active_;
  AwsAuthenticator awsAuthenticator_;
};

} // Http
} // Envoy