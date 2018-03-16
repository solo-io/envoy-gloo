#pragma once

#include <map>

#include "envoy/buffer/buffer.h"
#include "envoy/http/header_map.h"

// clang-format off
#include "json.hpp"
#include "inja.hpp"
// clang-format on

#include "transformation_filter.pb.h"

namespace Envoy {
namespace Http {

class ExtractorUtil {
public:
  static std::string
  extract(const envoy::api::v2::filter::http::Extraction &extractor,
          const HeaderMap &header_map);
};

class TransformerInstance {
public:
  TransformerInstance(const HeaderMap &header_map,
                      const std::map<std::string, std::string> &extractions,
                      const nlohmann::json &context);
  // header_value(name)
  // extracted_value(name, index)
  nlohmann::json header_callback(inja::Parsed::Arguments args,
                                 nlohmann::json data);

  nlohmann::json extracted_callback(inja::Parsed::Arguments args,
                                    nlohmann::json data);

  std::string render(const std::string &input);

  void useDotNotation() {
    env_.set_element_notation(inja::ElementNotation::Dot);
  }

private:
  inja::Environment env_;
  const HeaderMap &header_map_;
  const std::map<std::string, std::string> &extractions_;
  const nlohmann::json &context_;
};

class Transformer {
public:
  Transformer(
      const envoy::api::v2::filter::http::Transformation &transformation,
      bool advanced_templates);
  ~Transformer();

  void transform(HeaderMap &map, Buffer::Instance &body);

private:
  /*
    TransformerImpl& impl() { return reinterpret_cast<TransformerImpl&>(impl_);
    } const TransformerImpl& impl() const { return reinterpret_cast<const
    TransformerImpl &>(impl_); }

    static const size_t TransformerImplSize = 464;
    static const size_t TransformerImplAlign = 8;

    std::aligned_storage<TransformerImplSize, TransformerImplAlign>::type impl_;
  */
  const envoy::api::v2::filter::http::Transformation &transformation_;
  bool advanced_templates_{};
};

} // namespace Http
} // namespace Envoy
