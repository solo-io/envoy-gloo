#include "test/extensions/filters/http/aws_lambda/mocks.h"
#include "gmock/gmock.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {
using ::testing::ReturnRef;
using ::testing::Return;

MockStsContext::MockStsContext() {
  ON_CALL(*this, fetcher()).WillByDefault(ReturnRef(fetcher_));
  ON_CALL(*this, callbacks()).WillByDefault(Return(&callbacks_));
}

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
