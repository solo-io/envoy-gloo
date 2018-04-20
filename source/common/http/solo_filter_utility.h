#pragma once

#include "envoy/http/filter.h"
#include "envoy/upstream/cluster_manager.h"

namespace Envoy {
namespace Http {

/**
 * General utilities for HTTP filters.
 *
 * TODO(talnordan): Merge this class into
 * envoy/source/common.http/filter_utility.h.
 */
class SoloFilterUtility {
public:
  // TODO(talnordan): The envoyproxy/envoy convention seems to be not to
  // explicitly delete constructors.
  SoloFilterUtility() = delete;
  SoloFilterUtility(const SoloFilterUtility &) = delete;

  /**
   * Resolve the route entry.
   * @param decoder_callbacks supplies the decoder callback of filter.
   * @return the route entry selected for this request. Note: this will be
   * nullptr if no route was selected.
   */
  static const Router::RouteEntry *
  resolveRouteEntry(StreamFilterCallbacks *filter_callbacks);

  /**
   * Resolve the cluster name.
   * @param filter_callbacks supplies the encoder\decoder callback of filter.
   * @return the name of the cluster selected for this request. Note: this will
   * be nullptr if no route was selected.
   */
  static const std::string *
  resolveClusterName(StreamFilterCallbacks *filter_callbacks);
};

class PerFilterConfigUtilBase {
protected:
  PerFilterConfigUtilBase(const std::string &filter_name)
      : filter_name_(filter_name) {}
  const Router::RouteSpecificFilterConfig *
  getPerFilterBaseConfig(StreamFilterCallbacks &filter_callbacks);

private:
  const std::string &filter_name_;
  Router::RouteConstSharedPtr route_info_{};
};

template <class ConfigType>
class PerFilterConfigUtil : public PerFilterConfigUtilBase {

  static_assert(
      std::is_base_of<Router::RouteSpecificFilterConfig, ConfigType>::value,
      "ConfigType must be a subclass of Router::RouteSpecificFilterConfig");

public:
  PerFilterConfigUtil(const std::string &filter_name)
      : PerFilterConfigUtilBase(filter_name) {}

  const ConfigType *
  getPerFilterConfig(StreamFilterCallbacks &filter_callbacks) {
    return dynamic_cast<const ConfigType *>(
        getPerFilterBaseConfig(filter_callbacks));
  }
};

} // namespace Http
} // namespace Envoy
