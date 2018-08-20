#include "test/extensions/filters/http/aws_lambda/mocks.h"

#include "common/common/macros.h"

namespace Envoy {
namespace Http {

using testing::_;
using testing::Invoke;

MockFunctionRetriever::MockFunctionRetriever() {
  ON_CALL(*this, getFunction(_))
      .WillByDefault(Invoke([&](const MetadataAccessor &) {
        absl::optional<const std::string *> qualifier;
        if (!qualifier_.empty()) {
          qualifier = absl::optional<const std::string *>(&qualifier_);
        }
        return Function{&name_,   qualifier,    async_,      &host_,
                        &region_, &access_key_, &secret_key_};
      }));
}

MockFunctionRetriever::~MockFunctionRetriever() {}

} // namespace Http
} // namespace Envoy
