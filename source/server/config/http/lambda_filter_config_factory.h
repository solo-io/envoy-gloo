#pragma once

#include "common/config/lambda_well_known_names.h"

#include "extensions/filters/http/common/factory_base.h"
#include "lambda_filter.pb.validate.h"

namespace Envoy {
namespace Server {
namespace Configuration {

using Extensions::HttpFilters::Common::FactoryBase;

/**
 * Config registration for the AWS Lambda filter.
 */
class LambdaFilterConfigFactory
    : public FactoryBase<envoy::api::v2::filter::http::Lambda> {
public:
  LambdaFilterConfigFactory()
      : FactoryBase(Config::LambdaHttpFilterNames::get().LAMBDA) {}

private:
  Http::FilterFactoryCb createFilterFactoryFromProtoTyped(
      const envoy::api::v2::filter::http::Lambda &proto_config,
      const std::string &stats_prefix, FactoryContext &context) override;
};

} // namespace Configuration
} // namespace Server
} // namespace Envoy
