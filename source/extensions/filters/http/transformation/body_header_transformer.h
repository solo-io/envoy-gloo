#pragma once

#include <map>

#include "source/extensions/filters/http/transformation/transformer.h"
#include "transformer.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

class BodyHeaderTransformer : public Transformer {
public:
  BodyHeaderTransformer(bool add_request_metadata);
  void transform(Http::RequestOrResponseHeaderMap &map,
                 Http::RequestHeaderMap *request_headers,
                 Buffer::Instance &body,
                 Http::StreamFilterCallbacks &) const override;
  bool passthrough_body() const override { return false; };
  //RequestTransformerConstSharedPtr asRequestTransformer();

protected:
  bool add_request_metadata_{};
};

class BodyHeaderRequestTransformer : public RequestTransformer, public BodyHeaderTransformer {
public:
  BodyHeaderRequestTransformer(bool add_request_metadata);
  void transform(Http::RequestOrResponseHeaderMap &map,
                 Http::RequestHeaderMap *request_headers,
                 Buffer::Instance &body,
                 Http::StreamFilterCallbacks &cb) const override {
      BodyHeaderTransformer::transform(map, request_headers, body, cb);
  };
  bool passthrough_body() const override { return BodyHeaderTransformer::passthrough_body(); };
};

class BodyHeaderResponseTransformer : public ResponseTransformer, public BodyHeaderTransformer {
public:
  BodyHeaderResponseTransformer(bool add_request_metadata);
  void transform(Http::RequestOrResponseHeaderMap &map,
                 Http::RequestHeaderMap *request_headers,
                 Buffer::Instance &body,
                 Http::StreamFilterCallbacks &cb) const override {
      BodyHeaderTransformer::transform(map, request_headers, body, cb);
  };
  bool passthrough_body() const override { return BodyHeaderTransformer::passthrough_body(); };
};

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
