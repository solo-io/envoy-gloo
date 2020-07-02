#include "common/buffer/buffer_impl.h"

#include "extensions/filters/http/transformation/body_header_transformer.h"

#include "test/mocks/http/mocks.h"
#include "test/mocks/server/mocks.h"
#include "test/test_common/utility.h"

#include "fmt/format.h"
#include "nlohmann/json.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::_;
using testing::AtLeast;
using testing::Invoke;
using testing::Return;
using testing::ReturnPointee;
using testing::ReturnRef;
using testing::SaveArg;
using testing::WithArg;

using json = nlohmann::json;

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

TEST(BodyHeaderTransformer, transform) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {"x-test", "789"},
                                         {":path", "/users/123"}};
  Buffer::OwnedImpl body("testbody");

  BodyHeaderTransformer transformer;
  NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_{};
  transformer.transform(headers, &headers, body, filter_callbacks_);

  std::string res = body.toString();
  json actual = json::parse(res);
  auto expected = R"(
  {
    "headers" : {
      ":method": "GET",
      ":authority": "www.solo.io",
      "x-test": "789",
      ":path": "/users/123"
    },
    "body": "testbody"
  }
)"_json;

  EXPECT_EQ(expected, actual);
}

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
