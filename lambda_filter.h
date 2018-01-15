#pragma once

#include <map>
#include <string>

#include "envoy/upstream/cluster_manager.h"

#include "common/common/logger.h"

#include "server/config/network/http_connection_manager.h"

#include "aws_authenticator.h"
#include "function.h"
#include "lambda_filter.pb.h"

namespace Envoy {
namespace Http {

class LambdaFilterConfig {

  using ProtoConfig = envoy::api::v2::filter::http::Lambda;

public:
  LambdaFilterConfig(const ProtoConfig &proto_config);

  const std::string &aws_access() const { return aws_access_; }
  const std::string &aws_secret() const { return aws_secret_; }

private:
  const std::string aws_access_;
  const std::string aws_secret_;
};

typedef std::shared_ptr<LambdaFilterConfig> LambdaFilterConfigSharedPtr;

class LambdaFilter : public StreamDecoderFilter,
                     public Logger::Loggable<Logger::Id::filter> {
public:
  LambdaFilter(LambdaFilterConfigSharedPtr, ClusterFunctionMap);
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
  StreamDecoderFilterCallbacks *decoder_callbacks_;

  const std::string aws_access() const { return config_->aws_access(); }
  const std::string aws_secret() const { return config_->aws_secret(); }

  ClusterFunctionMap functions_;
  Function currentFunction_;
  void lambdafy();
  std::string functionUrlPath();

  Envoy::Http::HeaderMap *request_headers_{};
  bool active_;
  AwsAuthenticator awsAuthenticator_;
};

} // namespace Http
} // namespace Envoy
