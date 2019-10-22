#pragma once

#include "api/envoy/config/filter/http/transformation/v2/transformation_filter.pb.validate.h"
#include "envoy/http/header_map.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

class Matcher;
using MatcherConstPtr = std::shared_ptr<const Matcher>;

/**
 * Supports matching a HTTP requests with JWT requirements.
 */
class Matcher {
public:
  virtual ~Matcher() = default;

  /**
   * Returns if a HTTP request matches with the rules of the matcher.
   *
   * @param headers    the request headers used to match against. An empty map should be used if
   *                   there are none headers available.
   * @return  true if request is a match, false otherwise.
   */
  virtual bool matches(const Http::HeaderMap& headers) const PURE;

  /**
   * Factory method to create a shared instance of a matcher based on the rule defined.
   *
   * @param rule  the proto rule match message.
   * @return the matcher instance.
   */
  static MatcherConstPtr
  create(const ::envoy::api::v2::route::RouteMatch& match);
};


} // namespace JwtAuthn
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
