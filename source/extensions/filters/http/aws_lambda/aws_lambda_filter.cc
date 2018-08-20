#include "extensions/filters/http/aws_lambda/aws_lambda_filter.h"

#include <algorithm>
#include <list>
#include <string>
#include <vector>

#include "envoy/http/header_map.h"

#include "common/common/empty_string.h"
#include "common/common/hex.h"
#include "common/common/utility.h"
#include "common/http/filter_utility.h"
#include "common/http/headers.h"
#include "common/http/solo_filter_utility.h"
#include "common/http/utility.h"

#include "extensions/filters/http/aws_lambda_well_known_names.h"

namespace Envoy {
namespace Http {

const LowerCaseString AWSLambdaFilter::INVOCATION_TYPE("x-amz-invocation-type");
const std::string AWSLambdaFilter::INVOCATION_TYPE_EVENT("Event");
const std::string AWSLambdaFilter::INVOCATION_TYPE_REQ_RESP("RequestResponse");
const LowerCaseString AWSLambdaFilter::LOG_TYPE("x-amz-log-type");
const std::string AWSLambdaFilter::LOG_NONE("None");

AWSLambdaFilter::AWSLambdaFilter(Upstream::ClusterManager &cluster_manager)
    : cluster_manager_(cluster_manager) {}

AWSLambdaFilter::~AWSLambdaFilter() {}

std::string AWSLambdaFilter::functionUrlPath(const std::string &name,
                                             const std::string &qualifier) {

  std::stringstream val;
  val << "/2015-03-31/functions/" << name << "/invocations";
  if (!qualifier.empty()) {
    val << "?Qualifier=" << qualifier;
  }
  return val.str();
}

FilterHeadersStatus AWSLambdaFilter::decodeHeaders(HeaderMap &headers,
                                                   bool end_stream) {

  protocol_options_ = SoloFilterUtility::resolveProtocolOptions<
      const AWSLambdaProtocolExtensionConfig>(
      Config::AWSLambdaHttpFilterNames::get().AWS_LAMBDA, decoder_callbacks_,
      cluster_manager_);
  if (!protocol_options_) {
    return FilterHeadersStatus::Continue;
  }

  route_ = decoder_callbacks_->route();
  // great! this is an aws cluster. get the function information:
  function_on_route_ =
      SoloFilterUtility::resolvePerFilterConfig<AWSLambdaRouteConfig>(
          Config::AWSLambdaHttpFilterNames::get().AWS_LAMBDA, route_);

  if (!function_on_route_) {
    decoder_callbacks_->sendLocalReply(
        Code::NotFound, "no function present for AWS upstream", nullptr);
    return FilterHeadersStatus::StopIteration;
  }

  aws_authenticator_.init(&protocol_options_->access_key(),
                          &protocol_options_->secret_key());
  request_headers_ = &headers;

  request_headers_->insertMethod().value().setReference(
      Headers::get().MethodValues.Post);

  //  request_headers_->removeContentLength();
  request_headers_->insertPath().value(functionUrlPath(
      function_on_route_->name(), function_on_route_->qualifier()));

  if (end_stream) {
    lambdafy();
    return FilterHeadersStatus::Continue;
  }

  return FilterHeadersStatus::StopIteration;
}

FilterDataStatus AWSLambdaFilter::decodeData(Buffer::Instance &data,
                                             bool end_stream) {
  if (!function_on_route_) {
    return FilterDataStatus::Continue;
  }

  aws_authenticator_.updatePayloadHash(data);

  if (end_stream) {
    lambdafy();
    return FilterDataStatus::Continue;
  }

  return FilterDataStatus::StopIterationAndBuffer;
}

FilterTrailersStatus AWSLambdaFilter::decodeTrailers(HeaderMap &) {
  if (function_on_route_ != nullptr) {
    lambdafy();
  }

  return FilterTrailersStatus::Continue;
}

void AWSLambdaFilter::lambdafy() {
  static std::list<LowerCaseString> headers;

  const std::string &invocation_type = function_on_route_->async()
                                           ? INVOCATION_TYPE_EVENT
                                           : INVOCATION_TYPE_REQ_RESP;
  headers.push_back(INVOCATION_TYPE);
  request_headers_->addReference(INVOCATION_TYPE, invocation_type);

  headers.push_back(LOG_TYPE);
  request_headers_->addReference(LOG_TYPE, LOG_NONE);

  // TOOO(yuval-k) constify this and change the header list to
  // ref-or-inline like in header map
  headers.push_back(LowerCaseString("host"));
  request_headers_->insertHost().value(protocol_options_->host());

  headers.push_back(LowerCaseString("content-type"));

  aws_authenticator_.sign(request_headers_, std::move(headers),
                          protocol_options_->region());
  cleanup();
}

void AWSLambdaFilter::cleanup() {
  request_headers_ = nullptr;
  function_on_route_ = nullptr;
  protocol_options_.reset();
}

} // namespace Http
} // namespace Envoy
