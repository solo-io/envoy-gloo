#include "source/common/matcher/solo_matcher.h"

#include "source/common/common/logger.h"
#include "source/common/common/regex.h"
#include "source/common/router/config_impl.h"

#include "absl/strings/match.h"

using ::envoy::config::route::v3::RouteMatch;
using Envoy::Router::ConfigUtility;

namespace Envoy {
namespace Matcher {
namespace {

/**
 * Perform a match against any HTTP header or pseudo-header.
 */
class BaseMatcherImpl : public Matcher,
                        public Logger::Loggable<Logger::Id::filter> {
public:
  BaseMatcherImpl(const RouteMatch &match)
      : case_sensitive_(
            PROTOBUF_GET_WRAPPED_OR_DEFAULT(match, case_sensitive, true)),
        config_headers_(
            Http::HeaderUtility::buildHeaderDataVector(match.headers())) {
    for (const auto &query_parameter : match.query_parameters()) {
      config_query_parameters_.push_back(
          std::make_unique<Router::ConfigUtility::QueryParameterMatcher>(
              query_parameter));
    }
  }

  // Check match for HeaderMatcher and QueryParameterMatcher
  bool matchRoute(const Http::RequestHeaderMap &headers) const {
    bool matches = true;
    // TODO(potatop): matching on RouteMatch runtime is not implemented.

    matches &= Http::HeaderUtility::matchHeaders(headers, config_headers_);
    if (!config_query_parameters_.empty()) {
      Http::Utility::QueryParams query_parameters =
          Http::Utility::parseQueryString(
              headers.Path()->value().getStringView());
      matches &= ConfigUtility::matchQueryParams(query_parameters,
                                                 config_query_parameters_);
    }
    return matches;
  }

protected:
  const bool case_sensitive_;

private:
  std::vector<Http::HeaderUtility::HeaderDataPtr> config_headers_;
  std::vector<Router::ConfigUtility::QueryParameterMatcherPtr>
      config_query_parameters_;
};

/**
 * Perform a match against any path with prefix rule.
 */
class PrefixMatcherImpl : public BaseMatcherImpl {
public:
  PrefixMatcherImpl(const ::RouteMatch &match)
      : BaseMatcherImpl(match), prefix_(match.prefix()) {}

  bool matches(const Http::RequestHeaderMap &headers) const override {
    if (BaseMatcherImpl::matchRoute(headers) &&
        (case_sensitive_
             ? absl::StartsWith(headers.Path()->value().getStringView(),
                                prefix_)
             : absl::StartsWithIgnoreCase(
                   headers.Path()->value().getStringView(), prefix_))) {
      ENVOY_LOG(debug, "Prefix requirement '{}' matched.", prefix_);
      return true;
    }
    return false;
  }

private:
  // prefix string
  const std::string prefix_;
};

/**
 * Perform a match against any path with a specific path rule.
 */
class PathMatcherImpl : public BaseMatcherImpl {
public:
  PathMatcherImpl(const ::RouteMatch &match)
      : BaseMatcherImpl(match), path_(match.path()) {}

  bool matches(const Http::RequestHeaderMap &headers) const override {
    if (BaseMatcherImpl::matchRoute(headers)) {
      const Http::HeaderString &path = headers.Path()->value();
      const size_t compare_length =
          path.getStringView().length() -
          Http::Utility::findQueryStringStart(path).length();
      auto real_path = path.getStringView().substr(0, compare_length);
      bool match = case_sensitive_ ? real_path == path_
                                   : absl::EqualsIgnoreCase(real_path, path_);
      if (match) {
        ENVOY_LOG(debug, "Path requirement '{}' matched.", path_);
        return true;
      }
    }
    return false;
  }

private:
  // path string.
  const std::string path_;
};

/**
 * Perform a match against any path with a regex rule.
 * TODO(mattklein123): This code needs dedup with RegexRouteEntryImpl.
 */
class RegexMatcherImpl : public BaseMatcherImpl {
public:
  RegexMatcherImpl(const RouteMatch &match) : BaseMatcherImpl(match) {
    ASSERT(match.path_specifier_case() == RouteMatch::kSafeRegex);
    regex_ = Regex::Utility::parseRegex(match.safe_regex());
    regex_str_ = match.safe_regex().regex();
  }

  bool matches(const Http::RequestHeaderMap &headers) const override {
    if (BaseMatcherImpl::matchRoute(headers)) {
      const Http::HeaderString &path = headers.Path()->value();
      const absl::string_view query_string =
          Http::Utility::findQueryStringStart(path);
      absl::string_view path_view = path.getStringView();
      path_view.remove_suffix(query_string.length());
      if (regex_->match(path_view)) {
        ENVOY_LOG(debug, "Regex requirement '{}' matched.", regex_str_);
        return true;
      }
    }
    return false;
  }

private:

static Regex::CompiledMatcherPtr parseStdRegexAsCompiledMatcher(const std::string& regex,
                                 std::regex::flag_type flags = std::regex::optimize);
  Regex::CompiledMatcherPtr regex_;
  // raw regex string, for logging.
  std::string regex_str_;
};

class CompiledStdMatcher : public Regex::CompiledMatcher {
public:
  CompiledStdMatcher(std::regex&& regex) : regex_(std::move(regex)) {}

  // CompiledMatcher
  bool match(absl::string_view value) const override {
    try {
      return std::regex_match(value.begin(), value.end(), regex_);
    } catch (const std::regex_error& e) {
      return false;
    }
  }

  // CompiledMatcher
  std::string replaceAll(absl::string_view value, absl::string_view substitution) const override {
    try {
      return std::regex_replace(std::string(value), regex_, std::string(substitution));
    } catch (const std::regex_error& e) {
      return std::string(value);
    }
  }

private:
  const std::regex regex_;
};

} // namespace

MatcherConstPtr Matcher::create(const RouteMatch &match) {
  switch (match.path_specifier_case()) {
  case RouteMatch::PathSpecifierCase::kPrefix:
    return std::make_shared<PrefixMatcherImpl>(match);
  case RouteMatch::PathSpecifierCase::kPath:
    return std::make_shared<PathMatcherImpl>(match);
  case RouteMatch::PathSpecifierCase::kSafeRegex:
    return std::make_shared<RegexMatcherImpl>(match);
  // path specifier is required.
  case RouteMatch::PathSpecifierCase::PATH_SPECIFIER_NOT_SET:
  default:
    PANIC_DUE_TO_CORRUPT_ENUM;
  }
}

} // namespace Matcher
} // namespace Envoy
