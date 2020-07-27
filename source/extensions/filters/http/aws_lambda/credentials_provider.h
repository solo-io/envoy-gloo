#include <set>
#include <string>

#include "envoy/api/api.h"
#include "common/common/logger.h"
#include "common/singleton/const_singleton.h"
#include "extensions/common/aws/credentials_provider.h"


namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {

class ExtendedCredentialsProvider {
public:
  virtual ~ExtendedCredentialsProvider() = default;
  
  virtual Credentials getCredentials(std::shared_ptr<const AWSLambdaProtocolExtensionConfig> protocol_options) PURE;
}

namespace {
  constexpr char AWS_ROLE_ARN[] = "AWS_ROLE_ARN";
  constexpr char AWS_WEB_IDENTITY_TOKEN_FILE[] = "AWS_WEB_IDENTITY_TOKEN_FILE";
}


class StsConstantValues {
public:
  const std::string EndpointFormat{"https://sts.{}.amazonaws.com."};
};

using StsConstants = ConstSingleton<StsConstantValues>;


class StsCredentialsProvider :  public Common::Aws::CredentialsProvider,
                                public Logger::Loggable<Logger::Id::aws> {
public:  

  StsCredentialsProvider(Api::Api& api) : api_(api) {}

  Common::Aws::Credentials getCredentials() override {
    refreshIfNeeded();
    return cached_credentials_;
  }

protected:
  Api::Api& api_;
  SystemTime last_updated_;
  SystemTime expiration_time_;
  Common::Aws::Credentials cached_credentials_;

  void refreshIfNeeded();
  // bool needsRefresh()
  // void refresh()
};

class ProtocolOptionsCredentialProvider : public Common::Aws::CredentialsProvider,
                                          public Logger::Loggable<Logger::Id::aws> {
public:  

  ProtocolOptionsCredentialProvider(Api::Api& api) : api_(api) {}

  Common::Aws::Credentials getCredentials() override {
    refreshIfNeeded();
    return cached_credentials_;
  }

protected:

  void refreshIfNeeded();
  // bool needsRefresh()
  // void refresh()
};

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
