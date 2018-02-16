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
#include "common/http/utility.h"

#include "server/config/network/http_connection_manager.h"

namespace Envoy {
namespace Http {

const LowerCaseString LambdaFilter::INVOCATION_TYPE("x-amz-invocation-type");
const std::string LambdaFilter::INVOCATION_TYPE_EVENT("Event");
const std::string LambdaFilter::INVOCATION_TYPE_REQ_RESP("RequestResponse");
const LowerCaseString LambdaFilter::LOG_TYPE("x-amz-log-type");
const std::string LambdaFilter::LOG_NONE("None");

// TODO(yuval-k) can the config be removed?
LambdaFilter::LambdaFilter(Http::FunctionRetrieverSharedPtr retreiver,
                           Server::Configuration::FactoryContext &ctx,
                           const std::string &name,
                           LambdaFilterConfigSharedPtr config)
    : FunctionalFilterBase(ctx, name), config_(config),
      functionRetriever_(retreiver), cm_(ctx.clusterManager()) {}

LambdaFilter::~LambdaFilter() { cleanup(); }

std::string LambdaFilter::functionUrlPath() {

  const auto &currentFunction = currentFunction_.value();
  std::stringstream val;
  val << "/2015-03-31/functions/" << (*currentFunction.name_) << "/invocations";
  if (currentFunction.qualifier_.valid()) {
    val << "?Qualifier=" << (*currentFunction.qualifier_.value());
  }
  return val.str();
}

Envoy::Http::FilterHeadersStatus
LambdaFilter::functionDecodeHeaders(Envoy::Http::HeaderMap &headers,
                                    bool end_stream) {

  auto optionalFunction = functionRetriever_->getFunction(*this);
  if (!optionalFunction.valid()) {
    // This is ours to handle - return error to the user
    Utility::sendLocalReply(*decoder_callbacks_, is_reset_,
                            Code::InternalServerError,
                            "AWS Function not available");
    // Doing continue after a local reply will crash envoy
    return Envoy::Http::FilterHeadersStatus::StopIteration;
  }

  currentFunction_ = std::move(optionalFunction);
  const auto &currentFunction = currentFunction_.value();

  aws_authenticator_.init(currentFunction.access_key_,
                          currentFunction.secret_key_);
  request_headers_ = &headers;

  request_headers_->insertMethod().value().setReference(
      Envoy::Http::Headers::get().MethodValues.Post);

  //  request_headers_->removeContentLength();
  request_headers_->insertPath().value(functionUrlPath());

  if (end_stream) {
    lambdafy();
    return Envoy::Http::FilterHeadersStatus::Continue;
  }

  return Envoy::Http::FilterHeadersStatus::StopIteration;
}

Envoy::Http::FilterDataStatus
LambdaFilter::functionDecodeData(Envoy::Buffer::Instance &data,
                                 bool end_stream) {
  if (!currentFunction_.valid()) {
    return Envoy::Http::FilterDataStatus::Continue;
  }
  aws_authenticator_.updatePayloadHash(data);

  if (end_stream) {
    lambdafy();
    return Envoy::Http::FilterDataStatus::Continue;
  }

  return Envoy::Http::FilterDataStatus::StopIterationAndBuffer;
}

Envoy::Http::FilterTrailersStatus
LambdaFilter::functionDecodeTrailers(Envoy::Http::HeaderMap &) {
  if (currentFunction_.valid()) {
    lambdafy();
  }

  return Envoy::Http::FilterTrailersStatus::Continue;
}

void LambdaFilter::lambdafy() {
  static std::list<Envoy::Http::LowerCaseString> headers;

  const auto &currentFunction = currentFunction_.value();

  const std::string &invocation_type =
      currentFunction.async_ ? INVOCATION_TYPE_EVENT : INVOCATION_TYPE_REQ_RESP;
  headers.push_back(INVOCATION_TYPE);
  request_headers_->addReference(INVOCATION_TYPE, invocation_type);

  headers.push_back(LOG_TYPE);
  request_headers_->addReference(LOG_TYPE, LOG_NONE);

  // TOOO(yuval-k) constify this and change the header list to
  // ref-or-inline like in header map
  headers.push_back(Envoy::Http::LowerCaseString("host"));
  request_headers_->insertHost().value(*currentFunction.host_);

  headers.push_back(Envoy::Http::LowerCaseString("content-type"));

  aws_authenticator_.sign(request_headers_, std::move(headers),
                          *currentFunction.region_);
  cleanup();
}

void LambdaFilter::cleanup() {
  request_headers_ = nullptr;
  currentFunction_ = Optional<Function>();
}

} // namespace Http
} // namespace Envoy
