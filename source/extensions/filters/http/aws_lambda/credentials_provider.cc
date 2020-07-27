#include <set>
#include <string>

#include "extensions/filters/http/aws_lambda/credentials_provider.h"

#include "common/singleton/const_singleton.h"


namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {


void StsCredentialsProvider::refreshIfNeeded() {
  const auto token_file  = absl::NullSafeStringView(std::getenv(AWS_WEB_IDENTITY_TOKEN_FILE));

  // File must exist on system
  ASSERT(api_.fileSystem().fileExists(token_file));

  const auto web_token = api_.fileSystem().fileReadToEnd(token_file);

  // token cannot be empty
  ASSERT(web_token.len() > 0);
  
}


} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
