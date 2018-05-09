#include "common/http/functional_stream_decoder_base.h"

#include "envoy/registry/registry.h"

#include "common/config/solo_well_known_names.h"
#include "common/http/filter_utility.h"
#include "common/http/solo_filter_utility.h"
#include "common/protobuf/utility.h"

#include "extensions/filters/http/common/empty_http_filter_config.h"
#include "functional_base.pb.validate.h"

namespace Envoy {
namespace Http {

using Server::Configuration::FactoryContext;

FunctionRetrieverMetadataAccessor::~FunctionRetrieverMetadataAccessor() {}

absl::optional<const std::string *>
FunctionRetrieverMetadataAccessor::getFunctionName() const {
  RELEASE_ASSERT(function_name_);
  return function_name_;
}

absl::optional<const ProtobufWkt::Struct *>
FunctionRetrieverMetadataAccessor::getFunctionSpec() const {
  if (cluster_spec_ == nullptr) {
    return {};
  }
  return cluster_spec_;
}

absl::optional<const ProtobufWkt::Struct *>
FunctionRetrieverMetadataAccessor::getClusterMetadata() const {
  RELEASE_ASSERT(child_spec_);
  return child_spec_;
}

absl::optional<const ProtobufWkt::Struct *>
FunctionRetrieverMetadataAccessor::getRouteMetadata() const {

  if (route_spec_) {
    return route_spec_;
  }

  if (!route_info_) {
    // save the pointer as the metadata is owned by it.
    route_info_ = decoder_callbacks_->route();
    if (!route_info_) {
      return {};
    }
  }

  const Router::RouteEntry *routeEntry = route_info_->routeEntry();
  if (!routeEntry) {
    return {};
  }

  const auto &metadata = routeEntry->metadata();
  const auto filter_it = metadata.filter_metadata().find(childname_);
  if (filter_it != metadata.filter_metadata().end()) {
    route_spec_ = &filter_it->second;
    return route_spec_;
  }

  return {};
}

absl::optional<FunctionRetrieverMetadataAccessor::Result>
FunctionRetrieverMetadataAccessor::tryToGetSpec() {

  route_info_ = decoder_callbacks_->route();
  if (!route_info_) {
    return {};
  }

  const Router::RouteEntry *routeEntry = route_info_->routeEntry();
  if (!routeEntry) {
    return {};
  }

  fetchClusterInfoIfOurs();

  if (!cluster_info_) {
    return {};
  }

  // So now we know this this route is to a functional upstream. i.e. we must be
  // able to do a function route or error. unless passthrough is allowed on the
  // upstream.
  const auto *filter_config = SoloFilterUtility::resolvePerFilterConfig<
      FunctionalFilterMixinRouteFilterConfig>(
      Config::SoloCommonFilterNames::get().FUNCTIONAL_ROUTER, route_info_);
  if (!filter_config) {
    // check if we have metadata (i.e. gloo is not updated)
    // TODO(yuval-k): this will be removed in the future.
    auto legacy_result = tryToGetSpecLegacy(routeEntry);
    if (legacy_result.has_value()) {
      return legacy_result.value();
    }

    // this cast should never fail, but maybe we don't have a config...
    return canPassthrough()
               ? absl::optional<FunctionRetrieverMetadataAccessor::Result>()
               : Result::Error;
  }

  function_name_ = &filter_config->function_name_;
  tryToGetSpecFromCluster(*function_name_);

  return Result::Active;
}

absl::optional<FunctionRetrieverMetadataAccessor::Result>
FunctionRetrieverMetadataAccessor::tryToGetSpecLegacy(
    const Router::RouteEntry *routeEntry) {
  const auto &metadata = routeEntry->metadata();

  const auto filter_it = metadata.filter_metadata().find(
      Config::SoloCommonMetadataFilters::get().FUNCTIONAL_ROUTER);
  if (filter_it == metadata.filter_metadata().end()) {
    return {};
  }

  // this needs to have a field with the name of the cluster:
  const auto &filter_metadata_struct = filter_it->second;
  const auto &filter_metadata_struct_fields = filter_metadata_struct.fields();

  const auto cluster_it =
      filter_metadata_struct_fields.find(cluster_info_->name());
  if (cluster_it == filter_metadata_struct_fields.end()) {
    return Result::Error;
  }
  // the value is a struct with either a single function of multiple functions
  // with weights.
  const ProtobufWkt::Value &clustervalue = cluster_it->second;
  if (clustervalue.kind_case() != ProtobufWkt::Value::kStructValue) {
    return Result::Error;
  }
  const auto &clusterstruct = clustervalue.struct_value();

  for (auto &&fptr :
       {&FunctionRetrieverMetadataAccessor::findSingleFunction,
        &FunctionRetrieverMetadataAccessor::findMultileFunction}) {
    absl::optional<const std::string *> maybe_single_func =
        (this->*fptr)(clusterstruct);
    if (maybe_single_func.has_value()) {
      // we found a function so the search is over.
      function_name_ = maybe_single_func.value();
      cluster_spec_ = nullptr;
      tryToGetSpecFromCluster(*function_name_);

      return Result::Active;
    }
  }

  // Function not found :(
  // return a 404
  return Result::Error;
}

bool FunctionRetrieverMetadataAccessor::canPassthrough() {

  const auto &metadata = cluster_info_->metadata();

  const auto filter_it = metadata.filter_metadata().find(
      Config::SoloCommonMetadataFilters::get().FUNCTIONAL_ROUTER);
  if (filter_it == metadata.filter_metadata().end()) {
    return true;
  }
  const auto &filter_metadata_struct = filter_it->second;
  const auto &filter_metadata_fields = filter_metadata_struct.fields();

  const auto passthrough_it = filter_metadata_fields.find(
      Config::MetadataFunctionalRouterKeys::get().PASSTHROUGH);
  if (passthrough_it == filter_metadata_fields.end()) {
    return true;
  }

  const auto &functionsvalue = passthrough_it->second;
  if (functionsvalue.kind_case() != ProtobufWkt::Value::kBoolValue) {
    return true;
  }

  return functionsvalue.bool_value();
}

absl::optional<const std::string *>
FunctionRetrieverMetadataAccessor::findSingleFunction(
    const ProtobufWkt::Struct &filter_metadata_struct) {

  const auto &filter_metadata_fields = filter_metadata_struct.fields();

  // TODO: write code for multiple functions after e2e

  const auto function_it = filter_metadata_fields.find(
      Config::MetadataFunctionalRouterKeys::get().FUNCTION);
  if (function_it == filter_metadata_fields.end()) {
    return {};
  }

  const auto &value = function_it->second;
  if (value.kind_case() != ProtobufWkt::Value::kStringValue) {
    return {};
  }

  return absl::optional<const std::string *>(&value.string_value());
}

absl::optional<const std::string *>
FunctionRetrieverMetadataAccessor::findMultileFunction(
    const ProtobufWkt::Struct &filter_metadata_struct) {

  const auto &filter_metadata_fields = filter_metadata_struct.fields();

  const auto weighted_functions_it = filter_metadata_fields.find(
      Config::MetadataFunctionalRouterKeys::get().WEIGHTED_FUNCTIONS);
  if (weighted_functions_it == filter_metadata_fields.end()) {
    return {};
  }

  const auto &weighted_functions_value = weighted_functions_it->second;
  if (weighted_functions_value.kind_case() !=
      ProtobufWkt::Value::kStructValue) {
    return {};
  }
  const auto &weighted_functions_fields =
      weighted_functions_value.struct_value().fields();

  const auto total_weight_it = weighted_functions_fields.find(
      Config::MetadataFunctionalRouterKeys::get().FUNCTIONS_TOTAL_WEIGHT);
  if (total_weight_it == weighted_functions_fields.end()) {
    return {};
  }
  const auto &total_weight_value = total_weight_it->second;
  if (total_weight_value.kind_case() != ProtobufWkt::Value::kNumberValue) {
    return {};
  }
  uint64_t total_function_weight =
      static_cast<uint64_t>(total_weight_value.number_value());

  const auto functions_list_it = weighted_functions_fields.find(
      Config::MetadataFunctionalRouterKeys::get().FUNCTIONS);
  if (functions_list_it == weighted_functions_fields.end()) {
    return {};
  }
  const auto &functions_list_value = functions_list_it->second;
  if (functions_list_value.kind_case() != ProtobufWkt::Value::kListValue) {
    return {};
  }
  const auto &functions_list = functions_list_value.list_value();

  // TODO(yuval-k): factor this out for easier testing.
  // get a random number, and find the
  // algorithm is like in RouteEntryImplBase::clusterEntry
  uint64_t random_value = random_.random();
  uint64_t selected_value = random_value % total_function_weight;

  uint64_t begin = 0UL;
  uint64_t end = 0UL;

  for (const ProtobufWkt::Value &function : functions_list.values()) {

    auto maybe_fw = getFuncWeight(function);
    if (!maybe_fw.has_value()) {
      return {};
    }

    auto &&fw = maybe_fw.value();
    end = begin + fw.weight;
    if (((selected_value >= begin) && (selected_value < end)) ||
        (end >= total_function_weight)) {
      return absl::optional<const std::string *>(fw.name);
    }
    begin = end;
  }

  return {};
}

absl::optional<FunctionRetrieverMetadataAccessor::FunctionWeight>
FunctionRetrieverMetadataAccessor::getFuncWeight(
    const ProtobufWkt::Value &function_weight_value) {

  if (function_weight_value.kind_case() != ProtobufWkt::Value::kStructValue) {
    return {};
  }

  const auto &funcweight_fields = function_weight_value.struct_value().fields();
  const auto weight_it = funcweight_fields.find(
      Config::MetadataFunctionalRouterKeys::get().WEIGHTED_FUNCTIONS_WEIGHT);
  if (weight_it == funcweight_fields.end()) {
    return {};
  }
  const auto &weight_value = weight_it->second;
  if (weight_value.kind_case() != ProtobufWkt::Value::kNumberValue) {
    return {};
  }
  uint64_t weight = static_cast<uint64_t>(weight_value.number_value());

  const auto name_it = funcweight_fields.find(
      Config::MetadataFunctionalRouterKeys::get().WEIGHTED_FUNCTIONS_NAME);
  if (name_it == funcweight_fields.end()) {
    return {};
  }
  const auto &name_value = name_it->second;
  if (name_value.kind_case() != ProtobufWkt::Value::kStringValue) {
    return {};
  }

  return FunctionWeight{weight, &name_value.string_value()};
}

void FunctionRetrieverMetadataAccessor::tryToGetSpecFromCluster(
    const std::string &funcname) {

  const auto &metadata = cluster_info_->metadata();

  const auto filter_it = metadata.filter_metadata().find(
      Config::SoloCommonMetadataFilters::get().FUNCTIONAL_ROUTER);
  if (filter_it == metadata.filter_metadata().end()) {
    return;
  }
  const auto &filter_metadata_struct = filter_it->second;
  const auto &filter_metadata_fields = filter_metadata_struct.fields();

  const auto functions_it = filter_metadata_fields.find(
      Config::MetadataFunctionalRouterKeys::get().FUNCTIONS);
  if (functions_it == filter_metadata_fields.end()) {
    return;
  }

  const auto &functionsvalue = functions_it->second;
  if (functionsvalue.kind_case() != ProtobufWkt::Value::kStructValue) {
    return;
  }

  const auto &functions_struct = functionsvalue.struct_value();
  const auto &functions_struct_fields = functions_struct.fields();

  const auto spec_it = functions_struct_fields.find(funcname);
  if (spec_it == functions_struct_fields.end()) {
    return;
  }

  const auto &specvalue = spec_it->second;
  if (specvalue.kind_case() != ProtobufWkt::Value::kStructValue) {
    return;
  }

  cluster_spec_ = &specvalue.struct_value();
}

void FunctionRetrieverMetadataAccessor::fetchClusterInfoIfOurs() {

  if (cluster_info_) {
    return;
  }
  Upstream::ClusterInfoConstSharedPtr cluster_info =
      FilterUtility::resolveClusterInfo(decoder_callbacks_, cm_);

  if (!cluster_info) {
    return;
  }

  const auto &metadata = cluster_info->metadata();
  const auto filter_it = metadata.filter_metadata().find(childname_);
  if (filter_it != metadata.filter_metadata().end()) {
    // save the cluster info ptr locally as the specs lives in it.
    cluster_info_ = cluster_info;
    child_spec_ = &filter_it->second;
  }
}

// register the factory as it is a must if you use the base filter

class FunctionBaseFilterFactory
    : public Extensions::HttpFilters::Common::EmptyHttpFilterConfig {
public:
  // Server::Configuration::NamedHttpFilterConfigFactory
  std::string name() override {
    return Config::SoloCommonFilterNames::get().FUNCTIONAL_ROUTER;
  }

  // Server::Configuration::EmptyHttpFilterConfig
  Server::Configuration::HttpFilterFactoryCb
  createFilter(const std::string &, FactoryContext &) override {
    return [](Http::FilterChainFactoryCallbacks &) -> void {};
  }

  ProtobufTypes::MessagePtr createEmptyRouteConfigProto() override {
    return ProtobufTypes::MessagePtr{
        new envoy::api::v2::filter::http::FunctionalFilterRouteConfig()};
  }

  Router::RouteSpecificFilterConfigConstSharedPtr
  createRouteSpecificFilterConfig(const Protobuf::Message &source) override {
    const envoy::api::v2::filter::http::FunctionalFilterRouteConfig &cfg =
        MessageUtil::downcastAndValidate<
            const envoy::api::v2::filter::http::FunctionalFilterRouteConfig &>(
            source);
    auto obj = std::make_shared<Http::FunctionalFilterMixinRouteFilterConfig>();
    obj->function_name_ = cfg.function_name();
    return obj;
  }
};

/**
 * Static registration for the Google Cloud Functions filter. @see
 * RegisterFactory.
 */
static Registry::RegisterFactory<
    FunctionBaseFilterFactory,
    Server::Configuration::NamedHttpFilterConfigFactory>
    register_;

} // namespace Http
} // namespace Envoy
