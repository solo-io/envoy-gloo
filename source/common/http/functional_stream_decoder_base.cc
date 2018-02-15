#include "common/http/functional_stream_decoder_base.h"

#include "common/config/solo_well_known_names.h"
#include "common/http/filter_utility.h"
#include "common/http/solo_filter_utility.h"
#include "common/http/utility.h"

namespace Envoy {
namespace Http {

// TODO:
/*
  1. get route entry
  1. get the cluster object
  1. if the cluster object is not ours (the specific subclass function);
continue.
  2. get the name of the dest cluster.
  3. get the metadata()[func_filter][cluster_name][function]
  if not then
  3. get the metadata()[func_filter][cluster_name][functions]
  4. get destination function
  5. get the threadlocal cluster object, and its metadata
  6. find the funciton spec in the cluster metadata and return it.
  store the destination cluster info and a pointer to the spec inside it,
  and make it available to the sub class as a protected method.

  if no cluster or no function, continue iteration
  if cluster and function but not function spec, return error to downstream

// fast path: route has no route entry -> goodbye

*/

FunctionalFilterBase::~FunctionalFilterBase() {}

void FunctionalFilterBase::onDestroy() { is_reset_ = true; }

FilterHeadersStatus FunctionalFilterBase::decodeHeaders(HeaderMap &headers,
                                                        bool end_stream) {
  tryToGetSpec();
  if (error_) {
    // This means a local reply was sent, so no need to continue in the chain.
    return FilterHeadersStatus::StopIteration;
  }
  if (active()) {
    return functionDecodeHeaders(headers, end_stream);
  }
  return FilterHeadersStatus::Continue;
}

FilterDataStatus FunctionalFilterBase::decodeData(Buffer::Instance &data,
                                                  bool end_stream) {
  if (active()) {
    return functionDecodeData(data, end_stream);
  }
  return FilterDataStatus::Continue;
}

FilterTrailersStatus FunctionalFilterBase::decodeTrailers(HeaderMap &trailers) {
  if (active()) {
    return functionDecodeTrailers(trailers);
  }
  return FilterTrailersStatus::Continue;
}

const ProtobufWkt::Struct &FunctionalFilterBase::getFunctionSpec() const {
  RELEASE_ASSERT(cluster_spec_);
  return *cluster_spec_;
}

const ProtobufWkt::Struct &FunctionalFilterBase::getChildFilterSpec() const {
  RELEASE_ASSERT(child_spec_);
  return *child_spec_;
}

const ProtobufWkt::Struct *
FunctionalFilterBase::getChildRouteFilterSpec() const {

  if (route_spec_) {
    return route_spec_;
  }

  if (!route_info_) {
    // save the pointer as the metadata is owned by it.
    route_info_ = decoder_callbacks_->route();
    if (!route_info_) {
      return nullptr;
    }
  }

  const Envoy::Router::RouteEntry *routeEntry = route_info_->routeEntry();
  if (!routeEntry) {
    return nullptr;
  }

  const auto &metadata = routeEntry->metadata();
  const auto filter_it = metadata.filter_metadata().find(childname_);
  if (filter_it != metadata.filter_metadata().end()) {
    route_spec_ = &filter_it->second;
  }

  return route_spec_;
}

void FunctionalFilterBase::tryToGetSpec() {
  const Envoy::Router::RouteEntry *routeEntry =
      SoloFilterUtility::resolveRouteEntry(decoder_callbacks_);
  if (!routeEntry) {
    return;
  }

  fetchClusterInfoIfOurs();

  if (!cluster_info_) {
    return;
  }
  // So now we know this this route is to a functional upstream. i.e. we must be
  // able to do a function route or error.

  const auto &metadata = routeEntry->metadata();

  const auto filter_it = metadata.filter_metadata().find(
      Config::SoloFunctionalFilterMetadataFilters::get().FUNCTIONAL_ROUTER);
  if (filter_it == metadata.filter_metadata().end()) {
    error();
    return;
  }

  const auto &filter_metadata_struct = filter_it->second;
  findSingleFunction(filter_metadata_struct);

  if (active()) {
    // we have spec!
    return;
  }
  // TODO try multiple functions

  // Function not found :(
  // TODO: return a 404

  error();
}

void FunctionalFilterBase::findSingleFunction(
    const ProtobufWkt::Struct &filter_metadata_struct) {

  const auto &filter_metadata_fields = filter_metadata_struct.fields();

  // TODO: write code for multiple functions after e2e

  const auto function_it = filter_metadata_fields.find(
      Config::MetadataFunctionalRouterKeys::get().FUNCTION);
  if (function_it == filter_metadata_fields.end()) {
    return;
  }

  const auto &value = function_it->second;
  if (value.kind_case() != ProtobufWkt::Value::kStringValue) {
    return;
  }

  const std::string &funcname = value.string_value();

  tryToGetSpecFromCluster(funcname);
}

void FunctionalFilterBase::tryToGetSpecFromCluster(
    const std::string &funcname) {

  const auto &metadata = cluster_info_->metadata();

  const auto filter_it = metadata.filter_metadata().find(
      Config::SoloFunctionalFilterMetadataFilters::get().FUNCTIONAL_ROUTER);
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

  // save the cluster info as the spec lives in it.
  cluster_spec_ = &specvalue.struct_value();
}

void FunctionalFilterBase::fetchClusterInfoIfOurs() {

  if (cluster_info_) {
    return;
  }
  Upstream::ClusterInfoConstSharedPtr cluster_info =
      FilterUtility::resolveClusterInfo(decoder_callbacks_, cm_);

  const auto &metadata = cluster_info->metadata();
  const auto filter_it = metadata.filter_metadata().find(childname_);
  if (filter_it != metadata.filter_metadata().end()) {
    cluster_info_ = cluster_info;
    child_spec_ = &filter_it->second;
  }
}

void FunctionalFilterBase::error() {
  // error :(
  // free shared pointers.
  cluster_spec_ = nullptr;
  child_spec_ = nullptr;
  cluster_info_ = nullptr;
  route_spec_ = nullptr;
  route_info_ = nullptr;
  error_ = true;
  Utility::sendLocalReply(*decoder_callbacks_, is_reset_, Code::NotFound,
                          "Function not found");
}

} // namespace Http
} // namespace Envoy
