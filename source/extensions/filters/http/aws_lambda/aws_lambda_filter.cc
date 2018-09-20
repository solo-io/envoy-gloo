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
#include "common/singleton/const_singleton.h"

#include "extensions/filters/http/solo_well_known_names.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {

class AWSLambdaHeaderValues {
public:
  const Http::LowerCaseString InvocationType{"x-amz-invocation-type"};
  const std::string InvocationTypeEvent{"Event"};
  const std::string InvocationTypeRequestResponse{"RequestResponse"};
  const Http::LowerCaseString LogType{"x-amz-log-type"};
  const std::string LogNone{"None"};
  const Http::LowerCaseString HostHead{"x-amz-log-type"};
};

typedef ConstSingleton<AWSLambdaHeaderValues> AWSLambdaHeaderNames;

const HeaderList AWSLambdaFilter::HeadersToSign =
    AwsAuthenticator::createHeaderToSign(
        {AWSLambdaHeaderNames::get().InvocationType,
         AWSLambdaHeaderNames::get().LogType, Http::Headers::get().HostLegacy,
         Http::Headers::get().ContentType});

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

Http::FilterHeadersStatus
AWSLambdaFilter::decodeHeaders(Http::HeaderMap &headers, bool end_stream) {

  protocol_options_ = Http::SoloFilterUtility::resolveProtocolOptions<
      const AWSLambdaProtocolExtensionConfig>(
      SoloHttpFilterNames::get().AWS_LAMBDA, decoder_callbacks_,
      cluster_manager_);
  if (!protocol_options_) {
    return Http::FilterHeadersStatus::Continue;
  }

  route_ = decoder_callbacks_->route();
  // great! this is an aws cluster. get the function information:
  function_on_route_ =
      Http::SoloFilterUtility::resolvePerFilterConfig<AWSLambdaRouteConfig>(
          SoloHttpFilterNames::get().AWS_LAMBDA, route_);

  if (!function_on_route_) {
    decoder_callbacks_->sendLocalReply(
        Http::Code::NotFound, "no function present for AWS upstream", nullptr);
    return Http::FilterHeadersStatus::StopIteration;
  }

  aws_authenticator_.init(&protocol_options_->access_key(),
                          &protocol_options_->secret_key());
  request_headers_ = &headers;

  request_headers_->insertMethod().value().setReference(
      Http::Headers::get().MethodValues.Post);

  //  request_headers_->removeContentLength();
  request_headers_->insertPath().value(functionUrlPath(
      function_on_route_->name(), function_on_route_->qualifier()));

  if (end_stream) {
    lambdafy();
    return Http::FilterHeadersStatus::Continue;
  }

  return Http::FilterHeadersStatus::StopIteration;
}

Http::FilterDataStatus AWSLambdaFilter::decodeData(Buffer::Instance &data,
                                                   bool end_stream) {
  if (!function_on_route_) {
    return Http::FilterDataStatus::Continue;
  }

  aws_authenticator_.updatePayloadHash(data);

  if (end_stream) {
    lambdafy();
    return Http::FilterDataStatus::Continue;
  }

  return Http::FilterDataStatus::StopIterationAndBuffer;
}

Http::FilterTrailersStatus AWSLambdaFilter::decodeTrailers(Http::HeaderMap &) {
  if (function_on_route_ != nullptr) {
    lambdafy();
  }

  return Http::FilterTrailersStatus::Continue;
}

void AWSLambdaFilter::lambdafy() {

  const std::string &invocation_type =
      function_on_route_->async()
          ? AWSLambdaHeaderNames::get().InvocationTypeEvent
          : AWSLambdaHeaderNames::get().InvocationTypeRequestResponse;
  request_headers_->addReference(AWSLambdaHeaderNames::get().InvocationType,
                                 invocation_type);
  request_headers_->addReference(AWSLambdaHeaderNames::get().LogType,
                                 AWSLambdaHeaderNames::get().LogNone);
  request_headers_->insertHost().value(protocol_options_->host());

  aws_authenticator_.sign(request_headers_, HeadersToSign,
                          protocol_options_->region());
  cleanup();
}

void AWSLambdaFilter::cleanup() {
  request_headers_ = nullptr;
  function_on_route_ = nullptr;
  protocol_options_.reset();
}

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
