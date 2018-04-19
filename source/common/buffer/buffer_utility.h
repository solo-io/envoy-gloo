#pragma once

#include <string>

#include "envoy/buffer/buffer.h"

namespace Envoy {
namespace Buffer {

/**
 * General utilities for buffers.
 */
class BufferUtility {
public:
  /**
   * Drain a buffer to a string.
   * @param buffer supplies the buffer to drain.
   * @return std::string the converted string.
   */
  static std::string drainBufferToString(Buffer::Instance &buffer);

  /**
   * Convert a buffer to a string.
   * @param buffer supplies the buffer to convert.
   * @return std::string the converted string.
   */
  static std::string bufferToString(const Buffer::Instance &buffer);
};

} // namespace Buffer
} // namespace Envoy
