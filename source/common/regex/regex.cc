#include "source/common/regex/regex.h"

namespace Solo {
namespace Regex {

std::regex Utility::parseStdRegex(const std::string& regex, std::regex::flag_type flags) {
  try {
    return std::regex(regex, flags);
  } catch (const std::regex_error& e) {
    throw Envoy::EnvoyException(fmt::format("Invalid regex '{}': {}", regex, e.what()));
  }
}

} // namespace Regex
} // namespace Envoy
