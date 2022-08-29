#include "source/extensions/filters/http/aws_lambda/aws_lambda_filter.h"

#include <algorithm>
#include <list>
#include <string>
#include <vector>

#include "envoy/http/header_map.h"
#include "source/common/common/base64.h"

#include "source/common/buffer/buffer_impl.h"
#include "source/common/common/empty_string.h"
#include "source/common/common/hex.h"
#include "source/common/common/utility.h"
#include "source/common/http/headers.h"
#include "source/common/http/solo_filter_utility.h"
#include "source/common/http/utility.h"
#include "source/common/singleton/const_singleton.h"

#include "source/extensions/filters/http/solo_well_known_names.h"


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
  const Http::LowerCaseString FunctionError{"x-amz-function-error"};
};

typedef ConstSingleton<AWSLambdaHeaderValues> AWSLambdaHeaderNames;

const HeaderList AWSLambdaFilter::HeadersToSign =
    AwsAuthenticator::createHeaderToSign(
        {AWSLambdaHeaderNames::get().InvocationType,
         AWSLambdaHeaderNames::get().LogType, Http::Headers::get().HostLegacy,
         Http::Headers::get().ContentType});

AWSLambdaFilter::AWSLambdaFilter(Upstream::ClusterManager &cluster_manager,
                                 Api::Api &api,
                                 AWSLambdaConfigConstSharedPtr filter_config)
    : aws_authenticator_(api.timeSource()), cluster_manager_(cluster_manager),  
      filter_config_(filter_config){}

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

  request_headers_ = &headers;
  end_stream_ = end_stream;

  route_ = decoder_callbacks_->route();
  // great! this is an aws cluster. get the function information:
  function_on_route_ =
      Http::Utility::resolveMostSpecificPerFilterConfig<AWSLambdaRouteConfig>(
          decoder_callbacks_);

  if (!function_on_route_) {
    state_ = State::Responded;
    decoder_callbacks_->sendLocalReply(
        Http::Code::InternalServerError, RcDetails::get().FunctionNotFoundBody,
        nullptr, absl::nullopt, RcDetails::get().FunctionNotFound);
    return Http::FilterHeadersStatus::StopIteration;
  }

  // If the state is still the initial, attempt to get credentials
  ASSERT(state_ == State::Init);
  state_ = State::Calling;
  context_ = filter_config_->getCredentials(protocol_options_, this);

  if (state_ == State::Responded) {
    // An error was found, and a direct reply was set, make sure to stop
    // iteration
    return Http::FilterHeadersStatus::StopIteration;
  }

  if (context_ != nullptr) {
    // context exists, we're in async land
    // If the callback has not been processed, stop iteration
    if (state_ != State::Complete) {
      ENVOY_LOG(trace, "{}: stopping iteration to wait for STS credentials",
                __func__);
      stopped_ = true;
      return Http::FilterHeadersStatus::StopIteration;
    }
  }

  if (end_stream) {
    lambdafy();
    return Http::FilterHeadersStatus::Continue;
  }

  return Http::FilterHeadersStatus::StopIteration;
}


Http::FilterHeadersStatus 
AWSLambdaFilter::encodeHeaders(Http::ResponseHeaderMap &headers, bool end_stream) {

  if (!headers.get(AWSLambdaHeaderNames::get().FunctionError).empty()){
    // We treat upstream function errors as if it was any other upstream error
    headers.setStatus(504);
  }
  response_headers_ = &headers;
  if (isResponseTransformationNeeded() && !end_stream){
    // Stop iteration so that encodedata can mutate headers from alb json
    return Http::FilterHeadersStatus::StopIteration;
  }
  return Http::FilterHeadersStatus::Continue;
}

Http::FilterDataStatus AWSLambdaFilter::encodeData(
                                      Buffer::Instance &data, bool end_stream ){  

  if (state_ == State::Destroyed){
    // Safety against use after free if we exceed buffer limit 
    // during modifications. This should never happen as we only shrink.
    ENVOY_LOG(debug, "{}: attempted operations while destroyed", __func__);
    return Http::FilterDataStatus::StopIterationNoBuffer;
  }

  if (!isResponseTransformationNeeded()){
    // return response as is if not configured for alb mode/transformation
    return Http::FilterDataStatus::Continue;
  }

  if (!end_stream) {
    // we need the entire response prior to parsing and unwrapping the json
    return Http::FilterDataStatus::StopIterationAndBuffer;
  }
  encoder_callbacks_->addEncodedData(data, false);
  finalizeResponse();
  

  return Http::FilterDataStatus::Continue;
}

Http::FilterTrailersStatus 
AWSLambdaFilter::encodeTrailers(Http::ResponseTrailerMap &) {

  if (!isResponseTransformationNeeded()){
   return Http::FilterTrailersStatus::Continue;
  }
  // Future proof against alb http2 support and finalize the data transform
  finalizeResponse();
  
  return Http::FilterTrailersStatus::Continue;
}

void AWSLambdaFilter::finalizeResponse(){
  // Now that the response is finished we know that the following is safe
  // as the following options will only make the resulting buffer smaller.
  const Buffer::Instance&  buff = *encoder_callbacks_->encodingBuffer();
  encoder_callbacks_->modifyEncodingBuffer([this](Buffer::Instance& enc_buf) {
    Buffer::OwnedImpl body;
    if (functionOnRoute()->unwrapAsAlb()) {
      if (parseResponseAsALB(*response_headers_, enc_buf, body)){
        response_headers_->setStatus(static_cast<int>(Http::Code::InternalServerError));
        body.drain(body.length());
      }
    } else if (functionOnRoute()->hasTransformerConfig()) {
      auto transformer_config = functionOnRoute()->transformerConfig();
      transformer_config->transform(
        *response_headers_,
        request_headers_,
        enc_buf,
        *encoder_callbacks_
      );

      body.add(enc_buf.toString().c_str());
    }
    enc_buf.drain(enc_buf.length());
    enc_buf.move(body);
  });
  response_headers_->setContentLength(buff.length());
}

bool AWSLambdaFilter::parseResponseAsALB(Http::ResponseHeaderMap& headers, 
                const Buffer::Instance& json_buf, Buffer::Instance& body) {

  ProtobufWkt::Struct alb_response;                
  try {
    MessageUtil::loadFromJson(json_buf.toString(), alb_response);
  } catch (EnvoyException& ex) {
    ENVOY_LOG(debug, "{}: alb_unwrap set but did not recieve a json payload", 
                                                   functionOnRoute()->path());
    headers.setStatus(static_cast<int>(Http::Code::InternalServerError));
    return true;
  }
  
  const auto& flds = alb_response.fields();
  if (flds.contains("body")) {
    auto rawBody = flds.at("body").string_value();
    if (flds.contains("isBase64Encoded")){
      if (!flds.at("isBase64Encoded").has_bool_value()){
        return true;
      }
      if (flds.at("isBase64Encoded").bool_value()){
        rawBody = Base64::decode(rawBody);
      }
    }
    body.add(rawBody);
  }

  if (flds.contains("statusCode")){
    headers.setStatus(flds.at("statusCode").number_value());
    if (!flds.at("statusCode").has_number_value()){
      return true;
    }
  }
  if (flds.contains("headers")){
    const auto& hds = flds.at("headers").struct_value();
    const auto& hdsFields = hds.fields();
    for (auto const& hdrEntry : hdsFields) { 
      headers.addCopy(Http::LowerCaseString(hdrEntry.first),
                             hdrEntry.second.string_value());
    }
  }
  // While ALB would refuse to parse something with headers + multivalue
  // Being more permissive in this case was determined to be better.
  if (flds.contains("multiValueHeaders")){
    const auto& hds = flds.at("multiValueHeaders").struct_value();
    const auto& hdsFields = hds.fields();
    for (auto const& hdrEntry : hdsFields) { 
      const auto& list = hdrEntry.second.list_value();
      for (auto const& val : list.values()){
        headers.addCopy(Http::LowerCaseString(hdrEntry.first),
                                                            val.string_value());
      }
    }
  }
  return false;
}

bool AWSLambdaFilter::isResponseTransformationNeeded() {
  return functionOnRoute() != nullptr && (functionOnRoute()->unwrapAsAlb() || functionOnRoute()->hasTransformerConfig());
}

bool AWSLambdaFilter::isRequestTransformationNeeded() {
  return functionOnRoute() != nullptr && functionOnRoute()->hasRequestTransformerConfig();
}

void AWSLambdaFilter::onSuccess(
    std::shared_ptr<const Envoy::Extensions::Common::Aws::Credentials>
        credentials) {
  credentials_ = credentials;
  context_ = nullptr;
  state_ = State::Complete;

  const std::string *access_key{};
  const std::string *secret_key{};
  const std::string *session_token{};

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

  if ((access_key == nullptr) || (secret_key == nullptr)) {
    state_ = State::Responded;
    decoder_callbacks_->sendLocalReply(Http::Code::InternalServerError,
                                       RcDetails::get().CredentialsNotFoundBody,
                                       nullptr, absl::nullopt,
                                       RcDetails::get().CredentialsNotFound);
    return;
  }
  aws_authenticator_.init(access_key, secret_key, session_token);

  if (filter_config_->propagateOriginalRouting()){
    request_headers_->setEnvoyOriginalPath(request_headers_->getPathValue());
    request_headers_->addReferenceKey(Http::Headers::get().EnvoyOriginalMethod,
                                      request_headers_->getMethodValue());
  }

  request_headers_->setReferenceMethod(Http::Headers::get().MethodValues.Post);

  request_headers_->setReferencePath(function_on_route_->path());

  if (stopped_) {
    if (end_stream_) {
      // edge case where header only request was stopped, but now needs to be
      // lambdafied.
      lambdafy();
    }
    stopped_ = false;
    decoder_callbacks_->continueDecoding();
  }
}

// TODO: Use the failure status in the local reply
void AWSLambdaFilter::onFailure(CredentialsFailureStatus) {
  // cancel mustn't be called
  context_ = nullptr;
  state_ = State::Responded;
  decoder_callbacks_->sendLocalReply(
      Http::Code::InternalServerError, RcDetails::get().CredentialsNotFoundBody,
      nullptr, absl::nullopt, RcDetails::get().CredentialsNotFound);
}

Http::FilterDataStatus AWSLambdaFilter::decodeData(Buffer::Instance &data,
                                                   bool end_stream) {
  if (!function_on_route_) {
    return Http::FilterDataStatus::Continue;
  }
  end_stream_ = end_stream;

  if (data.length() != 0) {
    has_body_ = true;
  }

  aws_authenticator_.updatePayloadHash(data);

  if (state_ == Calling) {
    return Http::FilterDataStatus::StopIterationAndBuffer;
  } else if (state_ == Responded) {
    return Http::FilterDataStatus::StopIterationNoBuffer;
  }


  if (end_stream) {
    if (isRequestTransformationNeeded() && data.length() > 0) {
      transformRequest(data);
    }
    lambdafy();
    return Http::FilterDataStatus::Continue;
  }

  return Http::FilterDataStatus::StopIterationAndBuffer;
}

Http::FilterTrailersStatus
AWSLambdaFilter::decodeTrailers(Http::RequestTrailerMap &) {
  end_stream_ = true;
  if (state_ == State::Calling) {
    return Http::FilterTrailersStatus::StopIteration;
  } else if (state_ == Responded) {
    return Http::FilterTrailersStatus::StopIteration;
  }

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
}

void AWSLambdaFilter::handleDefaultBody() {
  if ((!has_body_) && function_on_route_->defaultBody()) {
    Buffer::OwnedImpl data(function_on_route_->defaultBody().value());
    if (isRequestTransformationNeeded()) {
      transformRequest(data);
    }

    request_headers_->setReferenceContentType(
        Http::Headers::get().ContentTypeValues.Json);
    request_headers_->setContentLength(data.length());
    aws_authenticator_.updatePayloadHash(data);
    decoder_callbacks_->addDecodedData(data, false);
  }
}

void AWSLambdaFilter::transformRequest(Buffer::Instance &data) {
  auto request_transformer_config = functionOnRoute()->requestTransformerConfig();
  request_transformer_config->transform(
    *request_headers_,
    request_headers_,
    data,
    *decoder_callbacks_
  );
}

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
