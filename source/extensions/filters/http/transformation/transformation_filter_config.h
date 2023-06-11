#pragma once

#include <string>

#include "envoy/router/router.h"
#include "envoy/config/typed_config.h"

#include "source/extensions/filters/http/solo_well_known_names.h"
#include "source/extensions/filters/http/transformation/transformer.h"
#include "source/extensions/filters/http/common/factory_base.h"

#include "api/envoy/config/filter/http/transformation/v2/transformation_filter.pb.validate.h"
#include "envoy/server/factory_context.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

class Transformation : public Envoy::Logger::Loggable<Envoy::Logger::Id::filter> {
public:
  static TransformerConstSharedPtr getTransformer(
      const envoy::api::v2::filter::http::Transformation &transformation,
      Server::Configuration::CommonFactoryContext &context );
};

class ResponseMatcher;
using ResponseMatcherConstPtr = std::unique_ptr<const ResponseMatcher>;
class ResponseMatcher {
public:
  virtual ~ResponseMatcher() = default;
  virtual bool matches(const Http::ResponseHeaderMap &headers,
                       const StreamInfo::StreamInfo &stream_info) const PURE;

  /**
   * Factory method to create a shared instance of a matcher based on the rule
   * defined.
   */
  static ResponseMatcherConstPtr
  create(const envoy::api::v2::filter::http::ResponseMatcher &match);
};

using TransformationConfigProto =
    envoy::api::v2::filter::http::FilterTransformations;
using RouteTransformationConfigProto =
    envoy::api::v2::filter::http::RouteTransformations;

class TransformationFilterConfig : public FilterConfig {
public:
  TransformationFilterConfig(const TransformationConfigProto &proto_config,
                             const std::string &prefix, Server::Configuration::FactoryContext &context);

  const std::vector<MatcherTransformerPair> &transformerPairs() const override {
    return transformer_pairs_;
  };

  std::string name() const override {
    return SoloHttpFilterNames::get().Transformation;
  }

  bool logRequestResponseInfo() const {
    return log_request_response_info_;
  }

private:
  // The list of transformer matchers.
  std::vector<MatcherTransformerPair> transformer_pairs_{};

  bool log_request_response_info_{};
};

class PerStageRouteTransformationFilterConfig : public TransformConfig {
public:
  PerStageRouteTransformationFilterConfig() = default;
  void addTransformation(
      const envoy::api::v2::filter::http::
          RouteTransformations_RouteTransformation &transformations,
          Server::Configuration::CommonFactoryContext &context);

  TransformerPairConstSharedPtr
  findTransformers(const Http::RequestHeaderMap &headers) const override;
  TransformerConstSharedPtr
  findResponseTransform(const Http::ResponseHeaderMap &,
                        StreamInfo::StreamInfo &) const override;

private:
  std::vector<MatcherTransformerPair> transformer_pairs_;
  std::vector<std::pair<ResponseMatcherConstPtr, TransformerConstSharedPtr>>
      response_transformations_;
};

class RouteTransformationFilterConfig : public RouteFilterConfig {
public:
  RouteTransformationFilterConfig(RouteTransformationConfigProto proto_config,
    Server::Configuration::ServerFactoryContext &context);
};


/**
 * Implemented for transformation extensions and registered via Registry::registerFactory or the
 * convenience class RegisterFactory.
 */
class TransformerExtensionFactory :  public Config::TypedFactory{
public:
  ~TransformerExtensionFactory() override = default;

/**
 * Create a particular transformation extension implementation from a config proto. If the
 * implementation is unable to produce a factory with the provided parameters, it should throw
 * EnvoyException. The returned pointer should never be nullptr.
 * @param config the custom configuration for this transformer extension type.
 */
  virtual TransformerConstSharedPtr createTransformer(const Protobuf::Message &config,
    google::protobuf::BoolValue log_request_response_info,
    Server::Configuration::CommonFactoryContext &context) PURE;

  virtual std::string name() const override PURE;

  std::string category() const override {return "io.solo.transformer"; }
};

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
