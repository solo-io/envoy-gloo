#pragma once

#include <map>
#include <string>

#include "envoy/http/filter.h"
#include "envoy/upstream/cluster_manager.h"

#include "extensions/filters/http/aws_lambda/aws_authenticator.h"
#include "extensions/filters/http/aws_lambda/config.h"
#include "extensions/filters/http/aws_lambda/sts_credentials_provider.h"

#include "api/envoy/config/filter/http/aws_lambda/v2/aws_lambda.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {

/*
 * A filter to make calls to AWS Lambda. Note that as a functional filter,
 * it expects retrieveFunction to be called before decodeHeaders.
 */
class AWSLambdaFilter : public Http::StreamDecoderFilter,
                        StsCredentialsProvider::Callbacks,
                        Logger::Loggable<Logger::Id::filter> {
public:
  AWSLambdaFilter(Upstream::ClusterManager &cluster_manager, Api::Api &api,
                  AWSLambdaConfigConstSharedPtr filter_config);
  ~AWSLambdaFilter();

  // Http::StreamFilterBase
  void onDestroy() override {
    // If context is still around, make sure to cancel it
    if (context_ != nullptr) {
      context_->cancel();
    }
  }

  // Http::StreamDecoderFilter
  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap &,
                                          bool) override;
  Http::FilterDataStatus decodeData(Buffer::Instance &, bool) override;
  Http::FilterTrailersStatus decodeTrailers(Http::RequestTrailerMap &) override;
  void setDecoderFilterCallbacks(
      Http::StreamDecoderFilterCallbacks &decoder_callbacks) override {
    decoder_callbacks_ = &decoder_callbacks;
  }

  void
  onSuccess(std::shared_ptr<const Envoy::Extensions::Common::Aws::Credentials>
                credential) override;
  void onFailure(CredentialsFailureStatus status) override;

private:
  static const HeaderList HeadersToSign;

  void handleDefaultBody();

  void lambdafy();

  Http::RequestHeaderMap *request_headers_{};
  AwsAuthenticator aws_authenticator_;

  Http::StreamDecoderFilterCallbacks *decoder_callbacks_{};

  Upstream::ClusterManager &cluster_manager_;
  std::shared_ptr<const AWSLambdaProtocolExtensionConfig> protocol_options_;

  Router::RouteConstSharedPtr route_;
  const AWSLambdaRouteConfig *function_on_route_{};
  bool has_body_{};

  AWSLambdaConfigConstSharedPtr filter_config_;

  CredentialsConstSharedPtr credentials_;

  ContextSharedPtr context_;
  // The state of the request
  enum State { Init, Calling, Responded, Complete };
  // The state of the get credentials request.
  State state_ = Init;
  // Whether or not iteration has been stopped to wait for the credentials
  // request
  bool stopped_{};

  // if end_stream_is true before stopping iteration
  bool end_stream_{};
};

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
