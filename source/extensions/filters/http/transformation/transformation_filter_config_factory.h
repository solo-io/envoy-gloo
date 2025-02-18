#pragma once

#include <string>

#include "envoy/server/filter_config.h"

#include "source/extensions/filters/http/common/factory_base.h"
#include "source/extensions/filters/http/solo_well_known_names.h"
#include "source/extensions/filters/http/transformation/transformation_filter_config.h"

#include "api/envoy/config/filter/http/transformation/v2/transformation_filter.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

// using Extensions::HttpFilters::Common::FactoryBase;

class TransformationFilterConfigFactory
    : public Common::DualFactoryBase<TransformationConfigProto,
                                 RouteTransformationConfigProto> {
public:
  TransformationFilterConfigFactory()
      : DualFactoryBase(SoloHttpFilterNames::get().Transformation) {}

private:
  absl::StatusOr<Http::FilterFactoryCb> createFilterFactoryFromProtoTyped(
      const TransformationConfigProto &proto_config,
      const std::string &stats_prefix, DualInfo info,
      Server::Configuration::ServerFactoryContext &context) override;

  absl::StatusOr<Router::RouteSpecificFilterConfigConstSharedPtr>
  createRouteSpecificFilterConfigTyped(
      const RouteTransformationConfigProto &,
      Server::Configuration::ServerFactoryContext &,
      ProtobufMessage::ValidationVisitor &) override;
};

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
