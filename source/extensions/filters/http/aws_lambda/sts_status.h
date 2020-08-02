#pragma once

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {

enum class CredentialsFailureStatus {
  /* A network error occurred causing STS credentials retrieval failure. */
  Network,
  /* A failure occurred when trying to parse the retrieved STS credential data. */
  InvalidSts,
  /* Token is expired. */
  ExpiredToken,
  /* Token is expired. */
  ClusterNotFound,
};

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
