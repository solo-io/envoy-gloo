#include "common/http/functional_stream_decoder_base.h"
#include "common/http/solo_filter_utility.h"
#include "common/http/filter_utility.h"

namespace Envoy {
namespace Http {

FunctionalFilterBase::~FunctionalFilterBase() {

}

FilterHeadersStatus FunctionalFilterBase::decodeHeaders(HeaderMap &headers, bool end_stream) {
    tryToGetSpec();
    if (spec_) {
        return functionDecodeHeaders(headers, end_stream);
    }
    return FilterHeadersStatus::Continue;
}

FilterDataStatus FunctionalFilterBase::decodeData(Buffer::Instance &data, bool end_stream) {
    if (spec_) {
        return functionDecodeData(data, end_stream);
    }
    return FilterDataStatus::Continue;
}

FilterTrailersStatus FunctionalFilterBase::decodeTrailers(HeaderMap &trailers) {
    if (spec_) {
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

    const envoy::api::v2::Metadata& metadata = routeEntry->metadata();
    // TODO CONSTIFY    
    const auto filter_it = metadata.filter_metadata().find("io.solo.function_router");
    if (filter_it == metadata.filter_metadata().end()) {
        return;
    }

    const auto &filter_metadata_struct = filter_it->second;
    const auto &filter_metadata_fields = filter_metadata_struct.fields();

    const auto function_it = filter_metadata_fields.find("function");
    if (function_it == filter_metadata_fields.end()) {
        return;
    }

    const auto &value = function_it->second;
    if (value.kind_case() != ProtobufWkt::Value::kStringValue) {
        return;
    }

    const std::string& funcname = value.string_value();

    tryToGetSpecFromCluster(funcname, info);
}

void FunctionalFilterBase::tryToGetSpecFromCluster(const std::string& funcname, const Upstream::ClusterInfoConstSharedPtr& clusterinfo) {


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

    // TODO: write code for multiple functions after e2e

}

bool FunctionalFilterBase::isOurCluster(const Upstream::ClusterInfoConstSharedPtr& clusterinfo) {
    const envoy::api::v2::Metadata& metadata = clusterinfo->metadata();
    const auto filter_it = metadata.filter_metadata().find(childname_);
    return filter_it != metadata.filter_metadata().end();
}


}
}