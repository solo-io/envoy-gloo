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
  bool passthrough_body() const override { return false; };

protected:
  bool add_request_metadata_{};
};

class BodyHeaderRequestTransformer : public RequestTransformer, public BodyHeaderTransformer {
public:
  BodyHeaderRequestTransformer(bool add_request_metadata);
  void transform(Http::RequestHeaderMap &request_headers,
                 Buffer::Instance &body,
                 Http::StreamFilterCallbacks &cb) const override;
  bool passthrough_body() const override { return BodyHeaderTransformer::passthrough_body(); };
};

class BodyHeaderResponseTransformer : public ResponseTransformer, public BodyHeaderTransformer {
public:
  BodyHeaderResponseTransformer();
  void transform(Http::ResponseHeaderMap &response_headers,
                 Http::RequestHeaderMap *request_headers,
                 Buffer::Instance &body,
                 Http::StreamFilterCallbacks &cb) const override;
  bool passthrough_body() const override { return BodyHeaderTransformer::passthrough_body(); };
};

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
