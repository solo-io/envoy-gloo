#pragma once

#include <string>

#include "envoy/server/filter_config.h"

#include "source/extensions/filters/http/common/factory_base.h"
#include "source/extensions/filters/http/solo_well_known_names.h"

#include "api/envoy/config/filter/http/upstream_wait/v2/upstream_wait_filter.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace UpstreamWait {

// using Extensions::HttpFilters::Common::FactoryBase;
using ::envoy::config::filter::http::upstream_wait::v2::UpstreamWaitFilterConfig;

class WaitFilterConfigFactory
    : public Common::DualFactoryBase<UpstreamWaitFilterConfig> {
public:
  WaitFilterConfigFactory()
      : DualFactoryBase(SoloHttpFilterNames::get().Wait) {}

private:
  absl::StatusOr<Http::FilterFactoryCb> createFilterFactoryFromProtoTyped(
      const UpstreamWaitFilterConfig &proto_config,
      const std::string &stats_prefix, DualInfo info,
      Server::Configuration::ServerFactoryContext &context) override;
};

} // namespace UpstreamWait
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
