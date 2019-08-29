#pragma once

#include <map>

#include "envoy/buffer/buffer.h"
#include "envoy/http/header_map.h"

#include "extensions/filters/http/transformation/transformer.h"

// clang-format off
#include "nlohmann/json.hpp"
#include "inja/inja.hpp"
// clang-format on

#include "api/envoy/config/filter/http/transformation/v2/transformation_filter.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

class TransformerInstance {
public:
  TransformerInstance(
      const Http::HeaderMap &header_map,
      const std::unordered_map<std::string, std::string> &extractions,
      const nlohmann::json &context);
  // header_value(name)
  // extracted_value(name, index)
  nlohmann::json header_callback(inja::Arguments args);

  nlohmann::json extracted_callback(inja::Arguments args);

  std::string render(const inja::Template &input);

private:
  inja::Environment env_;
  const Http::HeaderMap &header_map_;
  const std::unordered_map<std::string, std::string> &extractions_;
  const nlohmann::json &context_;
};

class Extractor {
public:
  Extractor(const envoy::api::v2::filter::http::Extraction &extractor);
  std::string extract(const Http::HeaderMap &header_map) const;

private:
  const Http::LowerCaseString headername_;
  const unsigned int group_;
  const std::regex extract_regex_;
};

class InjaTransformer : public Transformer {
public:
  InjaTransformer(const envoy::api::v2::filter::http::TransformationTemplate
                      &transformation);
  ~InjaTransformer();

  void transform(Http::HeaderMap &map, Buffer::Instance &body) const override;

private:
  /*
    TransformerImpl& impl() { return reinterpret_cast<TransformerImpl&>(impl_);
    } const TransformerImpl& impl() const { return reinterpret_cast<const
    TransformerImpl &>(impl_); }

    static const size_t TransformerImplSize = 464;
    static const size_t TransformerImplAlign = 8;

    std::aligned_storage<TransformerImplSize, TransformerImplAlign>::type impl_;
  */
  bool advanced_templates_{};
  std::vector<std::pair<std::string, Extractor>> extractors_;
  std::vector<std::pair<Http::LowerCaseString, inja::Template>> headers_;

  absl::optional<inja::Template> body_template_;
  bool merged_extractors_to_body_{};
};

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
