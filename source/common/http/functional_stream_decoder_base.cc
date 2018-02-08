#include "common/http/functional_stream_decoder_base.h"
#include "common/http/solo_filter_utility.h"
#include "common/http/filter_utility.h"
#include "common/http/utility.h"


namespace Envoy {
namespace Http {

    // TODO:
    /*
      1. get route entry
      1. get the cluster object
      1. if the cluster object is not ours (the specific subclass function); continue.
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

FunctionalFilterBase::~FunctionalFilterBase() {

}

FilterHeadersStatus FunctionalFilterBase::decodeHeaders(HeaderMap &headers, bool end_stream) {
    tryToGetSpec();
    if (active()) {
        return functionDecodeHeaders(headers, end_stream);
    }
    return FilterHeadersStatus::Continue;
}

FilterDataStatus FunctionalFilterBase::decodeData(Buffer::Instance &data, bool end_stream) {
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


const ProtobufWkt::Struct& FunctionalFilterBase::getFunctionSpec() {
    RELEASE_ASSERT(spec_);
    return *spec_;
}

void FunctionalFilterBase::tryToGetSpec() {
  const Envoy::Router::RouteEntry *routeEntry = SoloFilterUtility::resolveRouteEntry(decoder_callbacks_);
    if (!routeEntry) {
        return;
    }

  Upstream::ClusterInfoConstSharedPtr info =
      FilterUtility::resolveClusterInfo(decoder_callbacks_, cm_);

    if (!info) {
        return;
    }

    if (!isOurCluster(info)) {
        return;
    }
    // So now we know this this route is to a functional upstream. i.e. we must be able to do
    // a function route or error.

    const envoy::api::v2::Metadata& metadata = routeEntry->metadata();
    // TODO CONSTIFY    
    const auto filter_it = metadata.filter_metadata().find("io.solo.function_router");
    if (filter_it == metadata.filter_metadata().end()) {
        error();
        return;
    }

    const auto &filter_metadata_struct = filter_it->second;
    findSingleFunction(std::move(info), filter_metadata_struct);
    
    if (active()) {
        // we have spec!
        return;
    }
    // TODO try multiple functions

    // Function not found :(
    // TODO: return a 404

    error();

}

void FunctionalFilterBase::findSingleFunction(Upstream::ClusterInfoConstSharedPtr&& info, const ProtobufWkt::Struct& filter_metadata_struct) {

    const auto &filter_metadata_fields = filter_metadata_struct.fields();

    // TODO: write code for multiple functions after e2e

    const auto function_it = filter_metadata_fields.find("function");
    if (function_it == filter_metadata_fields.end()) {
        return;
    }

    const auto &value = function_it->second;
    if (value.kind_case() != ProtobufWkt::Value::kStringValue) {
        return;
    }

    const std::string& funcname = value.string_value();

    tryToGetSpecFromCluster(funcname, std::move(info));
}

void FunctionalFilterBase::tryToGetSpecFromCluster(const std::string& funcname, Upstream::ClusterInfoConstSharedPtr&& clusterinfo) {


    const envoy::api::v2::Metadata& metadata = clusterinfo->metadata();
    // TODO CONSTIFY
    const auto filter_it = metadata.filter_metadata().find("io.solo.function_router");
    if (filter_it == metadata.filter_metadata().end()) {
        return;
    }
    const auto &filter_metadata_struct = filter_it->second;
    const auto &filter_metadata_fields = filter_metadata_struct.fields();

    const auto functions_it = filter_metadata_fields.find("functions");
    if (functions_it == filter_metadata_fields.end()) {
        return;
    }

    const auto &functionsvalue = functions_it->second;
    if (functionsvalue.kind_case() != ProtobufWkt::Value::kStructValue) {
        return;
    }

    const auto& functions_struct = functionsvalue.struct_value();
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
    cluster_info_ = clusterinfo;
    spec_ = &specvalue.struct_value();
}

bool FunctionalFilterBase::isOurCluster(const Upstream::ClusterInfoConstSharedPtr& clusterinfo) {
    const envoy::api::v2::Metadata& metadata = clusterinfo->metadata();
    const auto filter_it = metadata.filter_metadata().find(childname_);
    return filter_it != metadata.filter_metadata().end();
}

void FunctionalFilterBase::error() {
      Utility::sendLocalReply(*decoder_callbacks_, false, Code::NotFound,
                                    "Function not found");

}


}
}