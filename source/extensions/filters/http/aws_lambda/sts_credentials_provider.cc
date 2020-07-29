#include "extensions/filters/http/aws_lambda/sts_credentials_provider.h"

#include "envoy/api/api.h"
#include "envoy/common/pure.h"
#include "envoy/common/time.h"
#include "extensions/common/aws/credentials_provider.h"
#include "api/envoy/config/filter/http/aws_lambda/v2/aws_lambda.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {

class ContextImpl : public StsCredentialsProvider::Context {
public:
  ContextImpl(Http::RequestHeaderMap& headers, Tracing::Span& parent_span,
              Verifier::Callbacks* callback)
      : headers_(headers), parent_span_(parent_span), callback_(callback) {}

  Http::RequestHeaderMap& headers() const override { return headers_; }

  Verifier::Callbacks* callback() const override { return callback_; }

  void cancel() override {
    for (const auto& it : auths_) {
      it->onDestroy();
    }
  }

  // Get Response data which can be used to check if a verifier node has responded or not.
  CompletionState& getCompletionState(const Verifier* verifier) {
    return completion_states_[verifier];
  }


private:
  Http::RequestHeaderMap& headers_;
  Verifier::Callbacks* callback_;
  std::unordered_map<const Verifier*, CompletionState> completion_states_;
  std::vector<AuthenticatorPtr> auths_;
};

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
