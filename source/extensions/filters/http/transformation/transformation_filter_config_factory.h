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

using Extensions::HttpFilters::Common::FactoryBase;

class TransformationFilterConfigFactory
    : public Common::FactoryBase<TransformationConfigProto,
                                 RouteTransformationConfigProto> {
public:
  TransformationFilterConfigFactory()
      : FactoryBase(SoloHttpFilterNames::get().Transformation) {}

private:
  Http::FilterFactoryCb createFilterFactoryFromProtoTyped(
      const TransformationConfigProto &proto_config,
      const std::string &stats_prefix,
      Server::Configuration::FactoryContext &context) override;

  Router::RouteSpecificFilterConfigConstSharedPtr
  createRouteSpecificFilterConfigTyped(
      const RouteTransformationConfigProto &,
      Server::Configuration::ServerFactoryContext &,
      ProtobufMessage::ValidationVisitor &) override;
};

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
