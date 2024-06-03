#include "source/extensions/filters/http/upstream_wait/config.h"

#include <string>

#include "envoy/registry/registry.h"

#include "source/common/common/macros.h"
#include "source/common/protobuf/utility.h"

#include "source/extensions/filters/http/upstream_wait/filter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace UpstreamWait {

absl::StatusOr<Http::FilterFactoryCb>
WaitFilterConfigFactory::createFilterFactoryFromProtoTyped(
    const UpstreamWaitFilterConfig &,
    const std::string &, DualInfo,
    Server::Configuration::ServerFactoryContext &) {

  return [](Http::FilterChainFactoryCallbacks &callbacks) -> void {
    auto waiter = new WaitingFilter();
    callbacks.addStreamDecoderFilter(Http::StreamDecoderFilterSharedPtr{waiter});
  };
}

/**
 * Static registration for this filter. @see RegisterFactory.
 */

REGISTER_FACTORY(WaitFilterConfigFactory,
                 Server::Configuration::UpstreamHttpFilterConfigFactory);

} // namespace UpstreamWait
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
