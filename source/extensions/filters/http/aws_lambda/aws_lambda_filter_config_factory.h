#pragma once

#include "envoy/upstream/upstream.h"

#include "extensions/filters/http/common/factory_base.h"
#include "extensions/filters/http/solo_well_known_names.h"

#include "api/envoy/config/filter/http/aws_lambda/v2/aws_lambda.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace AwsLambda {

/**
 * Config registration for the AWS Lambda filter.
 */
class AWSLambdaFilterConfigFactory
    : public Common::FactoryBase<
          envoy::config::filter::http::aws_lambda::v2::AWSLambdaConfig,
          envoy::config::filter::http::aws_lambda::v2::AWSLambdaPerRoute> {
public:
  AWSLambdaFilterConfigFactory()
      : FactoryBase(SoloHttpFilterNames::get().AwsLambda) {}

  Upstream::ProtocolOptionsConfigConstSharedPtr createProtocolOptionsConfig(
      const Protobuf::Message &config,
      Server::Configuration::ProtocolOptionsFactoryContext &) override;
  ProtobufTypes::MessagePtr createEmptyProtocolOptionsProto() override;

private:
  Http::FilterFactoryCb createFilterFactoryFromProtoTyped(
      const envoy::config::filter::http::aws_lambda::v2::AWSLambdaConfig
          &proto_config,
      const std::string &stats_prefix,
      Server::Configuration::FactoryContext &context) override;

  Router::RouteSpecificFilterConfigConstSharedPtr
  createRouteSpecificFilterConfigTyped(
      const envoy::config::filter::http::aws_lambda::v2::AWSLambdaPerRoute &,
      Server::Configuration::ServerFactoryContext &,
      ProtobufMessage::ValidationVisitor &) override;
};

} // namespace AwsLambda
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
