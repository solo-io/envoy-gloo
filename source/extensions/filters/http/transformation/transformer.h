#pragma once

#include <string>

#include "envoy/buffer/buffer.h"
#include "envoy/http/filter.h"
#include "envoy/http/header_map.h"
#include "envoy/router/router.h"

#include "source/common/protobuf/protobuf.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

class Transformer {
public:
  virtual ~Transformer() {}

  virtual bool passthrough_body() const PURE;

  virtual void transform(Http::RequestOrResponseHeaderMap &map,
                         // request header map. this has the request header map
                         // even when transforming responses.
                         Http::RequestHeaderMap *request_headers,
                         Buffer::Instance &body,
                         Http::StreamFilterCallbacks &callbacks) const PURE;
};

typedef std::shared_ptr<const Transformer> TransformerConstSharedPtr;

class TransformerPair {
public:
  TransformerPair(TransformerConstSharedPtr request_transformer,
                  TransformerConstSharedPtr response_transformer,
                  TransformerConstSharedPtr on_stream_completion_transformer,
                  bool should_clear_cache)
    : clear_route_cache_(should_clear_cache),
      request_transformation_(request_transformer),
      response_transformation_(response_transformer),
      on_stream_completion_transformation_(on_stream_completion_transformer) {}

  TransformerConstSharedPtr getRequestTranformation() const {
    return request_transformation_;
  }

  TransformerConstSharedPtr getResponseTranformation() const {
    return response_transformation_;
  }

  TransformerConstSharedPtr getOnStreamCompletionTransformation() const {
    return on_stream_completion_transformation_;
  }

  bool shouldClearCache() const { return clear_route_cache_; }

private:
  bool clear_route_cache_{};
  TransformerConstSharedPtr request_transformation_;
  TransformerConstSharedPtr response_transformation_;
  TransformerConstSharedPtr on_stream_completion_transformation_;
};
typedef std::shared_ptr<const TransformerPair> TransformerPairConstSharedPtr;

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
