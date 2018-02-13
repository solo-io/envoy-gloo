#include "test/mocks/lambda/mocks.h"

#include "common/common/macros.h"

namespace Envoy {
namespace Http {

using testing::Invoke;
using testing::_;

MockFunctionRetriever::MockFunctionRetriever() {
  ON_CALL(*this, getFunction(_))
      .WillByDefault(Invoke([&](const FunctionalFilterBase &) {
        return Function{&name_,   &qualifier_,  async_,      &host_,
                        &region_, &access_key_, &secret_key_};
      }));
}

MockFunctionRetriever::~MockFunctionRetriever() {}

} // namespace Http
} // namespace Envoy
