#pragma once

#include <memory>
#include <regex>

#include "envoy/common/regex.h"

namespace Solo {
namespace Regex {

/**
 * Utilities for constructing regular expressions.
 */
class Utility {
public:
  /**
   * Constructs a std::regex, converting any std::regex_error exception into an EnvoyException.
   * @param regex std::string containing the regular expression to parse.
   * @param flags std::regex::flag_type containing parser flags. Defaults to std::regex::optimize.
   * @return std::regex constructed from regex and flags.
   * @throw EnvoyException if the regex string is invalid.
   */
  static std::regex parseStdRegex(const std::string& regex,
                                  std::regex::flag_type flags = std::regex::optimize);

};

} // namespace Regex
} // namespace Envoy
