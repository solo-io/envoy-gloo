#pragma once

#include "common/config/lambda_well_known_names.h"

#include "extensions/filters/http/common/empty_http_filter_config.h"

namespace Envoy {
namespace Server {
namespace Configuration {

using Extensions::HttpFilters::Common::EmptyHttpFilterConfig;

/**
 * Config registration for the AWS Lambda filter.
 */
class LambdaFilterConfigFactory : public EmptyHttpFilterConfig {
public:
  LambdaFilterConfigFactory()
      : EmptyHttpFilterConfig(Config::LambdaHttpFilterNames::get().LAMBDA) {}

private:
  Http::FilterFactoryCb createFilter(const std::string &stat_prefix,
                                     FactoryContext &context) override;
};

} // namespace Configuration
} // namespace Server
} // namespace Envoy
