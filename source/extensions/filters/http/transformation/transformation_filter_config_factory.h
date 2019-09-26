#pragma once

#include <string>

#include "envoy/server/filter_config.h"

#include "extensions/filters/http/common/empty_http_filter_config.h"
#include "extensions/filters/http/common/factory_base.h"
#include "extensions/filters/http/solo_well_known_names.h"
#include "extensions/filters/http/transformation/transformation_filter_config.h"

#include "api/envoy/config/filter/http/transformation/v2/transformation_filter.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

using Extensions::HttpFilters::Common::FactoryBase;

class TransformationFilterConfigFactory : public Common::FactoryBase<TransformationConfigProto, RouteTransformationConfigProto> {
public:
  TransformationFilterConfigFactory()
      : FactoryBase(SoloHttpFilterNames::get().Transformation) {}

  ProtobufTypes::MessagePtr createEmptyRouteConfigProto() override;

private:

  Http::FilterFactoryCb createFilterFactoryFromProtoTyped(
      const TransformationConfigProto &proto_config, const std::string &stats_prefix,
      Server::Configuration::FactoryContext &context) override;

Router::RouteSpecificFilterConfigConstSharedPtr
    createRouteSpecificFilterConfigTyped(const RouteTransformationConfigProto&,
                                       Server::Configuration::FactoryContext&) override;
};

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
