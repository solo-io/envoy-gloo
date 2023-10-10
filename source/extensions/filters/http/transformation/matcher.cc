#include "api/envoy/config/filter/http/transformation/v2/transformation_filter.pb.validate.h"
#include "source/extensions/filters/http/transformation/matcher.h"
#include "source/common/matcher/matcher.h"
#include "source/extensions/filters/http/transformation/transformation_factory.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

class TransformationAction : public Envoy::Matcher::ActionBase<envoy::api::v2::filter::http::TransformationRule_Transformations> {
public:
  TransformationAction(TransformerPairConstSharedPtr transformations)
      : transformations_(std::move(transformations)) {}

  TransformerPairConstSharedPtr transformations() const { return transformations_; }

private:
  const std::string name_;
  TransformerPairConstSharedPtr transformations_;
};

TransformerPairConstSharedPtr matchTransform(Http::Matching::HttpMatchingDataImpl&& data, const Envoy::Matcher::MatchTreeSharedPtr<Http::HttpMatchingData>& matcher) {
    auto match = Matcher::evaluateMatch<Http::HttpMatchingData>(*matcher, data);
    if (match.result_) {
      auto action = match.result_();

      // The only possible action that can be used within the route matching context
      // is the TransformationAction, so this must be true.
      ASSERT(action->typeUrl() == TransformationAction::staticTypeUrl());
      ASSERT(dynamic_cast<TransformationAction*>(action.get()));
      const TransformationAction& transformation_action = static_cast<const TransformationAction&>(*action);

      return transformation_action.transformations();
    }
    return nullptr;
}

struct TransformationContext {
  Server::Configuration::ServerFactoryContext &factory_context;
};

class ActionFactory : public Envoy::Matcher::ActionFactory<TransformationContext> {
public:
  Envoy::Matcher::ActionFactoryCb
  createActionFactoryCb(const Protobuf::Message& config, TransformationContext& context,
                        ProtobufMessage::ValidationVisitor& validation_visitor) override;
  std::string name() const override { return "envoy.filters.transformation.action"; }
  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<envoy::api::v2::filter::http::TransformationRule_Transformations>();
  }
};

Envoy::Matcher::ActionFactoryCb
ActionFactory::createActionFactoryCb(const Protobuf::Message& config, TransformationContext& context,
                                     ProtobufMessage::ValidationVisitor& validation_visitor) {
  const auto& action_config =
      MessageUtil::downcastAndValidate<const envoy::api::v2::filter::http::TransformationRule_Transformations&>(config,
                                                                               validation_visitor);
TransformerPairConstSharedPtr transformations = createTransformations(action_config, context.factory_context);

  return [transformations]() { return std::make_unique<TransformationAction>(transformations); };
}

REGISTER_FACTORY(ActionFactory, Envoy::Matcher::ActionFactory<TransformationContext>);


class TransformationValidationVisitor
    : public Matcher::MatchTreeValidationVisitor<Http::HttpMatchingData> {
public:
  absl::Status performDataInputValidation(const Matcher::DataInputFactory<Http::HttpMatchingData>&,
                                          absl::string_view) override {
    return absl::OkStatus();
  }
};

Envoy::Matcher::MatchTreeSharedPtr<Http::HttpMatchingData> createTransformationMatcher(
    const xds::type::matcher::v3::Matcher &matcher_config,
    Server::Configuration::ServerFactoryContext &factory_context) {

  TransformationContext context{factory_context};
  TransformationValidationVisitor validation_visitor;
  Envoy::Matcher::MatchTreeFactory<Http::HttpMatchingData, TransformationContext> factory(
      context, factory_context, validation_visitor);
  auto matcher = factory.create(matcher_config)();

  if (!validation_visitor.errors().empty()) {
    throw EnvoyException(fmt::format("error creating transformation: {}",
                                     validation_visitor.errors()[0]));
  }
  return matcher;
}

}  // namespace Transformation
}  // namespace HttpFilters
}  // namespace Extensions
}  // namespace Envoy
