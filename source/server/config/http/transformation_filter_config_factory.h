#pragma once

#include <string>

#include "envoy/server/filter_config.h"

#include "extensions/filters/http/common/factory_base.h"

#include "transformation_filter.pb.validate.h"

namespace Envoy {
namespace Server {
namespace Configuration {

using Extensions::HttpFilters::Common::FactoryBase;

class TransformationFilterConfigFactory
    : public FactoryBase<envoy::api::v2::filter::http::Transformations,
                         envoy::api::v2::filter::http::RouteTransformations> {
public:
    TransformationFilterConfigFactory();

private:
  Http::FilterFactoryCb createFilterFactoryFromProtoTyped(
      const envoy::api::v2::filter::http::Transformations &proto_config,
      const std::string &stats_prefix, FactoryContext &context) override;

  Router::RouteSpecificFilterConfigConstSharedPtr
  createRouteSpecificFilterConfigTyped(
      const envoy::api::v2::filter::http::RouteTransformations &proto_config,
      FactoryContext &context) override;
};

} // namespace Configuration
} // namespace Server
} // namespace Envoy
