#pragma once

#include <string>

#include "envoy/buffer/buffer.h"
#include "envoy/http/header_map.h"
#include "envoy/router/router.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

class Transformer {
public:
  virtual ~Transformer() {}

  virtual bool passthrough_body() const PURE;

  virtual void transform(Http::HeaderMap &map,
                         Buffer::Instance &body) const PURE;
};

typedef std::shared_ptr<Transformer> TransformerSharedPtr;
typedef std::shared_ptr<const Transformer> TransformerConstSharedPtr;

class TransormConfig {
public:
  virtual ~TransormConfig() {}
  
  virtual TransformerConstSharedPtr getRequestTranformation() const PURE;
  virtual bool shouldClearCache() const PURE;
  virtual TransformerConstSharedPtr getResponseTranformation() const PURE;
};

class FilterConfig : public TransormConfig {
public:
    virtual std::string name() const PURE;   
};

class RouteFilterConfig : public Router::RouteSpecificFilterConfig, public TransormConfig {};

typedef std::shared_ptr<const RouteFilterConfig>
    RouteFilterConfigConstSharedPtr;
typedef std::shared_ptr<const FilterConfig>
    FilterConfigConstSharedPtr;

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
