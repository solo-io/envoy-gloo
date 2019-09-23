#pragma once

#include <map>

#include "envoy/buffer/buffer.h"
#include "envoy/http/header_map.h"

#include "extensions/filters/http/transformation/transformer.h"

#include "api/envoy/config/filter/http/transformation/v2/transformation_filter.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {


class DlpTransformer : public Transformer {
public:
  DlpTransformer(const envoy::api::v2::filter::http::DlpTransformation
                      &transformation);
  ~DlpTransformer();

  void transform(Http::HeaderMap &map, Buffer::Instance &body) const override;

private:
};

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
