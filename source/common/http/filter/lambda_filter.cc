#include "common/http/filter/lambda_filter.h"

#include <algorithm>
#include <list>
#include <string>
#include <vector>

#include "envoy/http/header_map.h"

#include "common/common/empty_string.h"
#include "common/common/hex.h"
#include "common/common/utility.h"
#include "common/http/filter_utility.h"
#include "common/http/solo_filter_utility.h"

#include "server/config/network/http_connection_manager.h"

namespace Envoy {
namespace Http {

LambdaFilter::LambdaFilter(Http::FunctionRetrieverSharedPtr retreiver,Server::Configuration::FactoryContext &ctx,
                           const std::string &name,
                           LambdaFilterConfigSharedPtr config)
    : FunctionalFilterBase(ctx, name), config_(config),
    functionRetriever_(retreiver),
    cm_(ctx.clusterManager()) {}

LambdaFilter::~LambdaFilter() {
  
  // No need to destruct as this has no state.
  // if this changes, need to make sure only to destruct
  // if it was initialized.
  //  aws_authenticator_.~AwsAuthenticator();
  
}

void LambdaFilter::onDestroy() {}

std::string LambdaFilter::functionUrlPath() {

  std::stringstream val;
  val << "/2015-03-31/functions/" << (*currentFunction_.name_)
      << "/invocations";
  return val.str();
}

Envoy::Http::FilterHeadersStatus LambdaFilter::functionDecodeHeaders(Envoy::Http::HeaderMap &headers,
                                    bool end_stream) {

  auto optionalFunction = functionRetriever_->getFunction(*this);
  if (!optionalFunction.valid()) {
    return Envoy::Http::FilterHeadersStatus::Continue;
  }

  currentFunction_ = std::move(optionalFunction.value());
  // placement new
  new(&aws_authenticator_) AwsAuthenticator(*currentFunction_.access_key_, *currentFunction_.secret_key_);

  headers.insertMethod().value().setReference(
      Envoy::Http::Headers::get().MethodValues.Post);

  //  headers.removeContentLength();
  headers.insertPath().value(functionUrlPath());
  request_headers_ = &headers;

  ENVOY_LOG(debug, "decodeHeaders called end = {}", end_stream);
  if (end_stream) {
    lambdafy();
    return Envoy::Http::FilterHeadersStatus::Continue;
  }
  
  return Envoy::Http::FilterHeadersStatus::StopIteration;
  
}

Envoy::Http::FilterDataStatus
LambdaFilter::functionDecodeData(Envoy::Buffer::Instance &data,
                                 bool end_stream) {

  // calc hash of data
  ENVOY_LOG(debug, "decodeData called end = {} data = {}", end_stream,
            data.length());

  aws_authenticator_.updatePayloadHash(data);

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
  request_headers_->insertHost().value(*currentFunction_.host_);

  headers.push_back(Envoy::Http::LowerCaseString("content-type"));

  aws_authenticator_.sign(request_headers_, std::move(headers),
                         *currentFunction_.region_);
  request_headers_ = nullptr;
}

Envoy::Http::FilterTrailersStatus
LambdaFilter::functionDecodeTrailers(Envoy::Http::HeaderMap &) {

  lambdafy();

  return Envoy::Http::FilterTrailersStatus::Continue;
}

} // namespace Http
} // namespace Envoy
