#pragma once

#include <map>
#include <string>

#include "envoy/http/filter.h"
#include "envoy/http/metadata_accessor.h"
#include "envoy/upstream/cluster_manager.h"

#include "extensions/filters/http/aws/aws_authenticator.h"
#include "extensions/filters/http/aws/function.h"
#include "extensions/filters/http/aws/function_retriever.h"

namespace Envoy {
namespace Http {

/*
 * A filter to make calls to AWS Lambda. Note that as a functional filter,
 * it expects retrieveFunction to be called before decodeHeaders.
 */
class LambdaFilter : public StreamDecoderFilter, public FunctionalFilter {
public:
  LambdaFilter(FunctionRetrieverSharedPtr retreiver);
  ~LambdaFilter();

  // Http::StreamFilterBase
  void onDestroy() override {}

  // Http::StreamDecoderFilter
  FilterHeadersStatus decodeHeaders(HeaderMap &, bool) override;
  FilterDataStatus decodeData(Buffer::Instance &, bool) override;
  FilterTrailersStatus decodeTrailers(HeaderMap &) override;
  void setDecoderFilterCallbacks(StreamDecoderFilterCallbacks &) override {}

  // Http::FunctionalFilter
  bool retrieveFunction(const MetadataAccessor &meta_accessor) override;

private:
  static const LowerCaseString INVOCATION_TYPE;
  static const std::string INVOCATION_TYPE_EVENT;
  static const std::string INVOCATION_TYPE_REQ_RESP;

  static const LowerCaseString LOG_TYPE;
  static const std::string LOG_NONE;

  FunctionRetrieverSharedPtr function_retriever_;

  absl::optional<Function> current_function_;
  void lambdafy();
  std::string functionUrlPath();
  void cleanup();

  HeaderMap *request_headers_{};
  AwsAuthenticator aws_authenticator_;
};

} // namespace Http
} // namespace Envoy
