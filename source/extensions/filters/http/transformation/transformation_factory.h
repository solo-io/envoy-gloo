#pragma once

#include <string>

#include "envoy/matcher/matcher.h"
#include "source/extensions/filters/http/transformation/transformer.h"
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

/**
 * Implemented for transformation extensions and registered via Registry::registerFactory or the
 * convenience class RegisterFactory.
 */
class TransformerExtensionFactory :  public Config::TypedFactory {
public:
  ~TransformerExtensionFactory() override = default;

/**
 * Create a particular transformation extension implementation from a config proto. If the
 * implementation is unable to produce a factory with the provided parameters, it should throw
 * EnvoyException. The returned pointer should never be nullptr.
 * @param config the custom configuration for this transformer extension type.
 */
  virtual TransformerConstSharedPtr createTransformer(const Protobuf::Message &config,
    Server::Configuration::CommonFactoryContext &context) PURE;

  virtual std::string name() const override PURE;

  std::string category() const override {return "io.solo.transformer"; }
};

std::unique_ptr<const TransformerPair> createTransformations(
    const envoy::api::v2::filter::http::TransformationRule_Transformations& route_transformation,
    Server::Configuration::CommonFactoryContext &context);

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
