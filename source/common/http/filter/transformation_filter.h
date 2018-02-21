#pragma once

#include "common/http/filter/transformation_filter_config.h"
#include "envoy/server/filter_config.h"

#include "transformation_filter.pb.h"

namespace Envoy {
namespace Http {

class TransformationFilter : public StreamDecoderFilter {
public:
  TransformationFilter(TransformationFilterConfigSharedPtr config);
  ~TransformationFilter();

  // Http::FunctionalFilterBase
  FilterHeadersStatus decodeHeaders(HeaderMap &, bool) override;
  FilterDataStatus decodeData(Buffer::Instance &, bool) override;
  FilterTrailersStatus decodeTrailers(HeaderMap &) override;
   
  void onDestroy() override {}
  void setDecoderFilterCallbacks(StreamDecoderFilterCallbacks& callbacks) override {
     callbacks_ = &callbacks;
   };
private:
TransformationFilterConfigSharedPtr config_;
StreamDecoderFilterCallbacks* callbacks_;

};

} // namespace Http
} // namespace Envoy
