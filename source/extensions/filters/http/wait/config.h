#pragma once

#include <string>

#include "envoy/server/filter_config.h"

#include "source/extensions/filters/http/common/factory_base.h"
#include "source/extensions/filters/http/solo_well_known_names.h"

#include "api/envoy/config/filter/http/wait/v2/wait_filter.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Wait {

// using Extensions::HttpFilters::Common::FactoryBase;
using ::envoy::config::filter::http::wait::v2::WaitFilterConfig;

class WaitFilterConfigFactory
    : public Common::DualFactoryBase<WaitFilterConfig> {
public:
  WaitFilterConfigFactory()
      : DualFactoryBase(SoloHttpFilterNames::get().Wait) {}

private:
  absl::StatusOr<Http::FilterFactoryCb> createFilterFactoryFromProtoTyped(
      const WaitFilterConfig &proto_config,
      const std::string &stats_prefix, DualInfo info,
      Server::Configuration::ServerFactoryContext &context) override;
};

} // namespace Wait
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
