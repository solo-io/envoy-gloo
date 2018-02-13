#include "test/mocks/lambda/mocks.h"

#include "common/common/macros.h"

namespace Envoy {
namespace Http {

using testing::Return;
using testing::_;

MockFunctionRetriever::MockFunctionRetriever() {
  ON_CALL(*this, getFunction(_))
      .WillByDefault(Return(Function{&name_, &qualifier_, async_, &host_,
                                     &region_, &access_key_, &secret_key_}));

  ON_CALL(*this, getFunctionFromSpec(_, _, _))
      .WillByDefault(Return(Function{&name_, &qualifier_, async_, &host_,
                                     &region_, &access_key_, &secret_key_}));
}

MockFunctionRetriever::~MockFunctionRetriever() {}

} // namespace Http
} // namespace Envoy
