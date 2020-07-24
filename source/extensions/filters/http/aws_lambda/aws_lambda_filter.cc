#include "extensions/filters/http/aws_lambda/aws_lambda_filter.h"

#include <algorithm>
#include <list>
#include <string>
#include <vector>

#include "envoy/http/header_map.h"

#include "common/buffer/buffer_impl.h"
#include "common/common/empty_string.h"
#include "common/common/hex.h"
#include "common/common/utility.h"
#include "common/http/headers.h"
#include "common/http/solo_filter_utility.h"
#include "common/http/utility.h"
#include "common/singleton/const_singleton.h"

#include "extensions/filters/http/solo_well_known_names.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {

namespace {
struct RcDetailsValues {
  const std::string FunctionNotFound = "aws_lambda_function_not_found";
  const std::string FunctionNotFoundBody =
      "no function present for AWS upstream";
  const std::string CredentialsNotFound = "aws_lambda_credentials_not_found";
  const std::string CredentialsNotFoundBody =
      "no credentials present for AWS upstream";
};
typedef ConstSingleton<RcDetailsValues> RcDetails;
} // namespace

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

AWSLambdaFilter::AWSLambdaFilter(Upstream::ClusterManager &cluster_manager,
                                 TimeSource &time_source,
                                 AWSLambdaConfigConstSharedPtr filter_config)
    : aws_authenticator_(time_source), cluster_manager_(cluster_manager),
      filter_config_(filter_config) {}

AWSLambdaFilter::~AWSLambdaFilter() {}

Http::FilterHeadersStatus
AWSLambdaFilter::decodeHeaders(Http::RequestHeaderMap &headers,
                               bool end_stream) {

  protocol_options_ = Http::SoloFilterUtility::resolveProtocolOptions<
      const AWSLambdaProtocolExtensionConfig>(
      SoloHttpFilterNames::get().AwsLambda, decoder_callbacks_,
      cluster_manager_);

  if (!protocol_options_) {
    return Http::FilterHeadersStatus::Continue;
  }

  const std::string *access_key{};
  const std::string *secret_key{};
  const std::string *session_token{};
  if (protocol_options_->accessKey().has_value() &&
      protocol_options_->secretKey().has_value()) {
    access_key = &protocol_options_->accessKey().value();
    secret_key = &protocol_options_->secretKey().value();
    // attempt to set session_token, ok if nil
    if (protocol_options_->sessionToken().has_value()) {
      session_token = &protocol_options_->secretKey().value();
    }
  } else if (filter_config_) {
    credentials_ = filter_config_->getCredentials();
    if (credentials_) {
      const absl::optional<std::string> &maybeAccessKeyId =
          credentials_->accessKeyId();
      const absl::optional<std::string> &maybeSecretAccessKey =
          credentials_->secretAccessKey();
      if (maybeAccessKeyId.has_value() && maybeSecretAccessKey.has_value()) {
        access_key = &maybeAccessKeyId.value();
        secret_key = &maybeSecretAccessKey.value();
      }
      if (credentials_->sessionToken().has_value()) {
        session_token = &credentials_->sessionToken().value();
      }
    }
  }

  if ((access_key == nullptr) || (secret_key == nullptr)) {
    decoder_callbacks_->sendLocalReply(Http::Code::InternalServerError,
                                       RcDetails::get().CredentialsNotFoundBody,
                                       nullptr, absl::nullopt,
                                       RcDetails::get().CredentialsNotFound);
    return Http::FilterHeadersStatus::StopIteration;
  }

  route_ = decoder_callbacks_->route();
  // great! this is an aws cluster. get the function information:
  function_on_route_ =
      Http::Utility::resolveMostSpecificPerFilterConfig<AWSLambdaRouteConfig>(
          SoloHttpFilterNames::get().AwsLambda, route_);

  if (!function_on_route_) {
    decoder_callbacks_->sendLocalReply(
        Http::Code::InternalServerError, RcDetails::get().FunctionNotFoundBody,
        nullptr, absl::nullopt, RcDetails::get().FunctionNotFound);
    return Http::FilterHeadersStatus::StopIteration;
  }

  aws_authenticator_.init(access_key, secret_key, session_token);
  request_headers_ = &headers;

  request_headers_->setReferenceMethod(Http::Headers::get().MethodValues.Post);

  request_headers_->setReferencePath(function_on_route_->path());

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

  if (data.length() != 0) {
    has_body_ = true;
  }

  aws_authenticator_.updatePayloadHash(data);

  if (end_stream) {
    lambdafy();
    return Http::FilterDataStatus::Continue;
  }

  return Http::FilterDataStatus::StopIterationAndBuffer;
}

Http::FilterTrailersStatus
AWSLambdaFilter::decodeTrailers(Http::RequestTrailerMap &) {
  if (function_on_route_ != nullptr) {
    lambdafy();
  }

  return Http::FilterTrailersStatus::Continue;
}

void AWSLambdaFilter::lambdafy() {

  handleDefaultBody();

  const std::string &invocation_type =
      function_on_route_->async()
          ? AWSLambdaHeaderNames::get().InvocationTypeEvent
          : AWSLambdaHeaderNames::get().InvocationTypeRequestResponse;
  request_headers_->addReference(AWSLambdaHeaderNames::get().InvocationType,
                                 invocation_type);
  request_headers_->addReference(AWSLambdaHeaderNames::get().LogType,
                                 AWSLambdaHeaderNames::get().LogNone);
  request_headers_->setReferenceHost(protocol_options_->host());

  aws_authenticator_.sign(request_headers_, HeadersToSign,
                          protocol_options_->region());
  cleanup();
}

void AWSLambdaFilter::handleDefaultBody() {
  if ((!has_body_) && function_on_route_->defaultBody()) {
    Buffer::OwnedImpl data(function_on_route_->defaultBody().value());

    request_headers_->setReferenceContentType(
        Http::Headers::get().ContentTypeValues.Json);
    request_headers_->setContentLength(data.length());
    aws_authenticator_.updatePayloadHash(data);
    decoder_callbacks_->addDecodedData(data, false);
  }
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
