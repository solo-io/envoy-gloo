#include <string>

#include "common/buffer/buffer_impl.h"
#include "common/buffer/buffer_utility.h"

#include "gtest/gtest.h"

namespace Envoy {
namespace Buffer {
namespace {

TEST(BufferUtilityTest, DrainBufferToString) {
  char input[] = "hello world";
  BufferFragmentImpl frag(input, 11, nullptr);
  Buffer::OwnedImpl buffer;
  buffer.addBufferFragment(frag);
  EXPECT_EQ(11, buffer.length());

  EXPECT_EQ(std::string(input), BufferUtility::drainBufferToString(buffer));
  EXPECT_EQ(0, buffer.length());

  EXPECT_EQ(std::string(""), buffer.toString());
  EXPECT_EQ(0, buffer.length());
}

} // namespace
} // namespace Buffer
} // namespace Envoy
