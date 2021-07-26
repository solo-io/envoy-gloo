#include "source/common/buffer/buffer_utility.h"

namespace Envoy {
namespace Buffer {

std::string BufferUtility::drainBufferToString(Buffer::Instance &buffer) {
  std::string output = buffer.toString();
  buffer.drain(buffer.length());
  return output;
}

} // namespace Buffer
} // namespace Envoy
