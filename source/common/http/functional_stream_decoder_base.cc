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
    const auto filter_it = metadata.filter_metadata().find("TODO:function filter name");
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

void FunctionalFilterBase::tryToGetSpecFromCluster(const std::string& , const Upstream::ClusterInfoConstSharedPtr& clusterinfo) {


    const envoy::api::v2::Metadata& metadata = clusterinfo->metadata();
    const auto filter_it = metadata.filter_metadata().find("TODO:function filter name");
    if (filter_it == metadata.filter_metadata().end()) {
        return;
    }
/*
    const auto &filter_metadata_struct = filter_it->second;
    const auto &filter_metadata_fields = filter_metadata_struct.fields();

    const auto function_it = filter_metadata_fields.find("function");
    if (function_it == fields.end()) {
        return;
    }
*/
// TODO finish logic
}

bool FunctionalFilterBase::isOurCluster(const Upstream::ClusterInfoConstSharedPtr& clusterinfo) {
    const envoy::api::v2::Metadata& metadata = clusterinfo->metadata();
    const auto filter_it = metadata.filter_metadata().find(childname_);
    return filter_it != metadata.filter_metadata().end();
}


}
}