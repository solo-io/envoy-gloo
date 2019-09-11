#pragma once

#include <map>
#include <string>

#include "envoy/http/filter.h"
#include "envoy/upstream/cluster_manager.h"

#include "extensions/filters/http/aws_lambda/aws_authenticator.h"
#include "extensions/filters/http/aws_lambda/config.h"

#include "api/envoy/config/filter/http/aws_lambda/v2/aws_lambda.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {

/*
 * A filter to make calls to AWS Lambda. Note that as a functional filter,
 * it expects retrieveFunction to be called before decodeHeaders.
 */
class AWSLambdaFilter : public Http::StreamDecoderFilter {
public:
  AWSLambdaFilter(Upstream::ClusterManager &cluster_manager,
                  TimeSource &time_source);
  ~AWSLambdaFilter();

  // Http::StreamFilterBase
  void onDestroy() override {}

  // Http::StreamDecoderFilter
  Http::FilterHeadersStatus decodeHeaders(Http::HeaderMap &, bool) override;
  Http::FilterDataStatus decodeData(Buffer::Instance &, bool) override;
  Http::FilterTrailersStatus decodeTrailers(Http::HeaderMap &) override;
  void setDecoderFilterCallbacks(
      Http::StreamDecoderFilterCallbacks &decoder_callbacks) override {
    decoder_callbacks_ = &decoder_callbacks;
  }

private:
  static const HeaderList HeadersToSign;

  void handleDefaultBody();

  void lambdafy();
  void cleanup();

  Http::HeaderMap *request_headers_{};
  AwsAuthenticator aws_authenticator_;

  Http::StreamDecoderFilterCallbacks *decoder_callbacks_{};

  Upstream::ClusterManager &cluster_manager_;
  std::shared_ptr<const AWSLambdaProtocolExtensionConfig> protocol_options_;

  Router::RouteConstSharedPtr route_;
  const AWSLambdaRouteConfig *function_on_route_{};
  bool has_body_{};
};

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
