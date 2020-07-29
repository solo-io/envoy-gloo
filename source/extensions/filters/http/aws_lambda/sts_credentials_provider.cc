#include "extensions/filters/http/aws_lambda/sts_credentials_provider.h"

#include "envoy/api/api.h"
#include "envoy/common/pure.h"
#include "envoy/common/time.h"
#include "extensions/common/aws/credentials_provider.h"
#include "extensions/filters/http/aws_lambda/sts_fetcher.h"

#include "api/envoy/config/filter/http/aws_lambda/v2/aws_lambda.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {

class ContextImpl : public StsCredentialsProvider::Context {
public:
  ContextImpl(Http::RequestHeaderMap& headers, StsCredentialsProvider::Callbacks* callback)
      : headers_(headers), callback_(callback) {}

  Http::RequestHeaderMap& headers() const override { return headers_; }

  StsCredentialsProvider::Callbacks* callback() const override { return callback_; }

  void cancel() override {
    fetcher_->cancel();
  }


private:
  Http::RequestHeaderMap& headers_;
  StsCredentialsProvider::Callbacks* callback_;
  StsFetcherPtr fetcher_;
};

class StsCredentialsProviderImpl: public StsCredentialsProvider {
public:
  StsCredentialsProviderImpl(
    const envoy::config::filter::http::aws_lambda::v2::AWSLambdaConfig_ServiceAccountCredentials& config,
    Api::Api& api) : api_(api), config_(config) {};

  ContextSharedPtr find(const std::string&){return nullptr;};
private:

  Api::Api& api_;
  const envoy::config::filter::http::aws_lambda::v2::AWSLambdaConfig_ServiceAccountCredentials& config_;
};


StsCredentialsProviderPtr
StsCredentialsProvider::create(
  const envoy::config::filter::http::aws_lambda::v2::AWSLambdaConfig_ServiceAccountCredentials& config, Api::Api& api) {

  return std::make_unique<StsCredentialsProviderImpl>(config, api);
};

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
