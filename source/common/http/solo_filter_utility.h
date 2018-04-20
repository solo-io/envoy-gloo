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

/**
 * Base class that retrieves the current route config. Do not use directly. use
 * via PerFilterConfigUtil.
 */
class PerFilterConfigUtilBase {
protected:
  /**
   * @param filter_name The name of the filter who's route config should be
   * fetched. NOTE: the filter name is not copied - the provided string must out
   * live the instance of this class.
   */
  PerFilterConfigUtilBase(const std::string &filter_name)
      : filter_name_(filter_name) {}

  /**
   * Gets the route config. first from the routeEntry(), if not found thene from
   * the route(), and finally from routeEntry()->virtualhost().
   *
   * @return The route config if found. nullptr if not found. The returned
   * pointer is guaranteed to live atleast until the next invocaiton of this
   * method.
   */
  const Router::RouteSpecificFilterConfig *
  getPerFilterBaseConfig(StreamFilterCallbacks &filter_callbacks);

private:
  // The filter name who's config is needed
  const std::string &filter_name_;
  // Pointer to the route object. it is saved here to guarantee the lifetime of
  // the returned config.
  Router::RouteConstSharedPtr route_info_{};
};

/**
 * A helper class to get a perfilter config. If your filter has a perfilter
 * config, you can use this class to access it in a uniform way.
 *
 * Use it as a member in your filter class:
 *
 *     class MyFilter : public StreamDecoderFilter {
 *       ...
 *       PerFilterConfigUtil<MyFilterConfig> per_filter_config_;
 *     };
 *
 *    MyFilter::decodeHeaders(...) {
 *      auto&& cfg = per_filter_config_.getPerFilterConfig(callbacks_)
 *    }
 *
 */
template <class ConfigType>
class PerFilterConfigUtil : public PerFilterConfigUtilBase {

  static_assert(
      std::is_base_of<Router::RouteSpecificFilterConfig, ConfigType>::value,
      "ConfigType must be a subclass of Router::RouteSpecificFilterConfig");

public:
  /**
   * @param filter_name The name of the filter who's route config should be
   * fetched. NOTE: the filter name is not copied - the provided string must out
   * live the instance of this class.
   */
  PerFilterConfigUtil(const std::string &filter_name)
      : PerFilterConfigUtilBase(filter_name) {}

  /**
   * Gets the route config. first from the routeEntry(), if not found thene from
   * the route(), and finally from routeEntry()->virtualhost().
   *
   * @return The route config if found. nullptr if not found. The returned
   * pointer is guaranteed to live atleast until the next invocaiton of this
   * method.
   */
  const ConfigType *
  getPerFilterConfig(StreamFilterCallbacks &filter_callbacks) {
    return dynamic_cast<const ConfigType *>(
        getPerFilterBaseConfig(filter_callbacks));
  }
};

} // namespace Http
} // namespace Envoy
