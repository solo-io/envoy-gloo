#include "lambda_filter.h"

#include <algorithm>
#include <list>
#include <string>
#include <vector>

#include "envoy/http/header_map.h"

#include "common/common/empty_string.h"
#include "common/common/hex.h"
#include "common/common/utility.h"

#include "server/config/network/http_connection_manager.h"

namespace Envoy {
namespace Http {

LambdaFilterConfig::LambdaFilterConfig(const ProtoConfig &proto_config)
    : aws_access_(proto_config.access_key()),
      aws_secret_(proto_config.secret_key()) {}

LambdaFilter::LambdaFilter(LambdaFilterConfigSharedPtr config,
                           ClusterFunctionMap functions)
    : config_(config), functions_(std::move(functions)), active_(false),
      awsAuthenticator_(aws_access(), aws_secret(),
                        std::move(std::string("lambda"))) {}

LambdaFilter::~LambdaFilter() {}

void LambdaFilter::onDestroy() {}

std::string LambdaFilter::functionUrlPath() {

  std::stringstream val;
  val << "/2015-03-31/functions/" << currentFunction_.func_name_
      << "/invocations";
  return val.str();
}

Envoy::Http::FilterHeadersStatus
LambdaFilter::decodeHeaders(Envoy::Http::HeaderMap &headers, bool end_stream) {

  const Envoy::Router::RouteEntry *routeEntry =
      decoder_callbacks_->route()->routeEntry();

  if (routeEntry == nullptr) {
    return Envoy::Http::FilterHeadersStatus::Continue;
  }

  const std::string &cluster_name = routeEntry->clusterName();
  ClusterFunctionMap::iterator currentFunction = functions_.find(cluster_name);
  if (currentFunction == functions_.end()) {
    return Envoy::Http::FilterHeadersStatus::Continue;
  }

  active_ = true;
  currentFunction_ = currentFunction->second;

  headers.insertMethod().value().setReference(
      Envoy::Http::Headers::get().MethodValues.Post);

  //  headers.removeContentLength();
  headers.insertPath().value(functionUrlPath());
  request_headers_ = &headers;

  ENVOY_LOG(debug, "decodeHeaders called end = {}", end_stream);

  return Envoy::Http::FilterHeadersStatus::StopIteration;
}

Envoy::Http::FilterDataStatus
LambdaFilter::decodeData(Envoy::Buffer::Instance &data, bool end_stream) {

  if (!active_) {
    return Envoy::Http::FilterDataStatus::Continue;
  }
  // calc hash of data
  ENVOY_LOG(debug, "decodeData called end = {} data = {}", end_stream,
            data.length());

  awsAuthenticator_.update_payload_hash(data);

  if (end_stream) {

    lambdafy();
    // Authorization: AWS4-HMAC-SHA256
    // Credential=AKIDEXAMPLE/20150830/us-east-1/iam/aws4_request,
    // SignedHeaders=content-type;host;x-amz-date,
    // Signature=5d672d79c15b13162d9279b0855cfba6789a8edb4c82c400e06b5924a6f2b5d7
    // add header ?!
    // get stream id
    return Envoy::Http::FilterDataStatus::Continue;
  }

  return Envoy::Http::FilterDataStatus::StopIterationAndBuffer;
}

void LambdaFilter::lambdafy() {
  std::list<Envoy::Http::LowerCaseString> headers;

  headers.push_back(Envoy::Http::LowerCaseString("x-amz-invocation-type"));
  request_headers_->addCopy(
      Envoy::Http::LowerCaseString("x-amz-invocation-type"),
      std::string("RequestResponse"));

  //  headers.push_back(Envoy::Http::LowerCaseString("x-amz-client-context"));
  //  request_headers_->addCopy(Envoy::Http::LowerCaseString("x-amz-client-context"),
  //  std::string(""));

  headers.push_back(Envoy::Http::LowerCaseString("x-amz-log-type"));
  request_headers_->addCopy(Envoy::Http::LowerCaseString("x-amz-log-type"),
                            std::string("None"));

  headers.push_back(Envoy::Http::LowerCaseString("host"));
  request_headers_->insertHost().value(currentFunction_.hostname_);

  headers.push_back(Envoy::Http::LowerCaseString("content-type"));

  awsAuthenticator_.sign(request_headers_, std::move(headers),
                         currentFunction_.region_);
  request_headers_ = nullptr;
  active_ = false;
}

Envoy::Http::FilterTrailersStatus
LambdaFilter::decodeTrailers(Envoy::Http::HeaderMap &) {
  if (active_) {
    lambdafy();
  }

  return Envoy::Http::FilterTrailersStatus::Continue;
}

void LambdaFilter::setDecoderFilterCallbacks(
    Envoy::Http::StreamDecoderFilterCallbacks &callbacks) {
  decoder_callbacks_ = &callbacks;
}

} // namespace Http
} // namespace Envoy
