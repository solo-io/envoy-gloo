#pragma once

#include "envoy/http/filter.h"

#include "common/common/assert.h"
#include "common/http/solo_filter_utility.h"

namespace Envoy {
namespace Http {

template <typename T>
class RouteEnabledFilterWrapper : public StreamDecoderFilter {
  static_assert(std::is_base_of<StreamDecoderFilter, T>::value,
                "T should be a base of StreamDecoderFilter");

public:
  template <class... Ts>
  RouteEnabledFilterWrapper(const std::string &name, Ts &&... args)
      : inner_(std::forward<Ts>(args)...), name_(name) {}
  virtual ~RouteEnabledFilterWrapper() {}

  // Http::StreamFilterBase
  void onDestroy() override { inner_.onDestroy(); }

  // Http::StreamDecoderFilter
  FilterHeadersStatus decodeHeaders(HeaderMap &headers,
                                    bool end_stream) override {
    const Router::RouteEntry *re =
        SoloFilterUtility::resolveRouteEntry(decoder_callbacks_);
    if (re != nullptr) {
      const auto &filtersmeta = re->metadata().filter_metadata();
      const auto filter_it = filtersmeta.find(name_);
      if (filter_it != filtersmeta.end()) {
        active_ = true;
        return inner_.decodeHeaders(headers, end_stream);
      }
    }
    return FilterHeadersStatus::Continue;
  }

  FilterDataStatus decodeData(Buffer::Instance &data,
                              bool end_stream) override {
    if (active_) {
      return inner_.decodeData(data, end_stream);
    }
    return FilterDataStatus::Continue;
  }

  FilterTrailersStatus decodeTrailers(HeaderMap &trailers) override {
    if (active_) {
      return inner_.decodeTrailers(trailers);
    }
    return FilterTrailersStatus::Continue;
  }

  void setDecoderFilterCallbacks(
      StreamDecoderFilterCallbacks &decoder_callbacks) override {
    decoder_callbacks_ = &decoder_callbacks;
    inner_.setDecoderFilterCallbacks(decoder_callbacks);
  }

private:
  // Inner filter.
  T inner_;
  // Inner filter name.
  const std::string &name_;
  // Are we active or not.
  bool active_{false};
  // Callbacks to get the route metadata.
  StreamDecoderFilterCallbacks *decoder_callbacks_{};
};

} // namespace Http
} // namespace Envoy
