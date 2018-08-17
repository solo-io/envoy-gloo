#pragma once

#include <map>
#include <string>

#include "envoy/http/filter.h"
#include "envoy/http/metadata_accessor.h"
#include "envoy/upstream/cluster_manager.h"

#include "extensions/filters/http/aws_lambda/aws_authenticator.h"
#include "api/envoy/config/filter/http/aws/v2/lambda.pb.validate.h"

namespace Envoy {
namespace Http {

class LambdaRouteConfig : public Router::RouteSpecificFilterConfig {
public:
  LambdaRouteConfig(const envoy::config::filter::http::aws::v2::LambdaPerRoute& protoconfig) :
  name_(protoconfig.name()),
  qualifier_(protoconfig.qualifier()),
  async_(protoconfig.async())
  {
  }

  const std::string& name() const {return name_;}
  const std::string& qualifier() const {return qualifier_;}
  bool async() const {return async_;}

private:
  std::string name_;
  std::string qualifier_;
  bool async_;

};

class LambdaProtocolExtensionConfig : public Upstream::ProtocolOptionsConfig {
  public:
  LambdaProtocolExtensionConfig(const envoy::config::filter::http::aws::v2::LambdaProtocolExtension& protoconfig) :
  host_(protoconfig.host()),
  region_(protoconfig.region()),
  access_key_(protoconfig.access_key()),
  secret_key_(protoconfig.secret_key())
  {}

  const std::string& host() const {return host_;}
  const std::string& region() const {return region_;}
  const std::string& access_key() const {return access_key_;}
  const std::string& secret_key() const {return secret_key_;}
private:
  std::string host_;
  std::string region_;
  std::string access_key_;
  std::string secret_key_;
};

/*
 * A filter to make calls to AWS Lambda. Note that as a functional filter,
 * it expects retrieveFunction to be called before decodeHeaders.
 */
class AWSLambdaFilter : public StreamDecoderFilter {
public:

  AWSLambdaFilter(Upstream::ClusterManager& cluster_manager);
  ~AWSLambdaFilter();

  // Http::StreamFilterBase
  void onDestroy() override {}

  // Http::StreamDecoderFilter
  FilterHeadersStatus decodeHeaders(HeaderMap &, bool) override;
  FilterDataStatus decodeData(Buffer::Instance &, bool) override;
  FilterTrailersStatus decodeTrailers(HeaderMap &) override;
  void setDecoderFilterCallbacks(StreamDecoderFilterCallbacks &decoder_callbacks) override {
    decoder_callbacks_ = &decoder_callbacks;
  }

private:
  static const LowerCaseString INVOCATION_TYPE;
  static const std::string INVOCATION_TYPE_EVENT;
  static const std::string INVOCATION_TYPE_REQ_RESP;

  static const LowerCaseString LOG_TYPE;
  static const std::string LOG_NONE;

  void lambdafy();
  std::string functionUrlPath();
  void cleanup();

  HeaderMap *request_headers_{};
  AwsAuthenticator aws_authenticator_;

  StreamDecoderFilterCallbacks *decoder_callbacks_{};

  Upstream::ClusterManager& cluster_manager_;
  std::shared_ptr<const LambdaProtocolExtensionConfig> protocol_options_;
  
  Router::RouteConstSharedPtr route_;
  const LambdaRouteConfig* function_on_route_{};
};

} // namespace Http
} // namespace Envoy
