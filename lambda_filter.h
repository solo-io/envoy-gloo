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

class LambdaFilterConfig {
public:
  LambdaFilterConfig(Envoy::Runtime::Loader& runtimeloader);
  const std::string& aws_access();
  const std::string& aws_secret();
  const Function* get_function(const std::string& cluster);

private:
  const ClusterFunctionMap functions_;
  Envoy::Runtime::Loader& runtimeloader_;
};

typedef std::shared_ptr<LambdaFilterConfig> LambdaFilterConfigSharedPtr;

class LambdaFilter : public Envoy::Http::StreamDecoderFilter,  public Envoy::Logger::Loggable<Envoy::Logger::Id::filter> {
public:
  LambdaFilter(std::string access_key, std::string secret_key, ClusterFunctionMap functions);
  ~LambdaFilter();

  // Http::StreamFilterBase
  void onDestroy() override;

  // Http::StreamDecoderFilter
  Envoy::Http::FilterHeadersStatus decodeHeaders(Envoy::Http::HeaderMap& headers, bool) override;
  Envoy::Http::FilterDataStatus decodeData(Envoy::Buffer::Instance&, bool) override;
  Envoy::Http::FilterTrailersStatus decodeTrailers(Envoy::Http::HeaderMap&) override;
  void setDecoderFilterCallbacks(Envoy::Http::StreamDecoderFilterCallbacks& callbacks) override;

private:
  Envoy::Http::StreamDecoderFilterCallbacks* decoder_callbacks_;
  ClusterFunctionMap functions_;
  Function currentFunction_;
  void lambdafy();
  std::string functionUrlPath();

  Envoy::Http::HeaderMap* request_headers_{};
  bool active_;
  AwsAuthenticator awsAuthenticator_;
};

} // Lambda
} // Solo