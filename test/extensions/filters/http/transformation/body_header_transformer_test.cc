#include "common/buffer/buffer_impl.h"

#include "extensions/filters/http/transformation/body_header_transformer.h"

#include "test/test_common/utility.h"

#include "fmt/format.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::AtLeast;
using testing::Invoke;
using testing::Return;
using testing::ReturnPointee;
using testing::ReturnRef;
using testing::SaveArg;
using testing::WithArg;
using testing::_;

using json = nlohmann::json;

namespace Envoy {
namespace Http {

TEST(BodyHeaderTransformer, transform) {
  TestHeaderMapImpl headers{{":method", "GET"},
                            {":authority", "www.solo.io"},
                            {"x-test", "789"},
                            {":path", "/users/123"}};
  Buffer::OwnedImpl body("testbody");

  BodyHeaderTransformer transformer;
  transformer.transform(headers, body);

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

} // namespace Http
} // namespace Envoy
