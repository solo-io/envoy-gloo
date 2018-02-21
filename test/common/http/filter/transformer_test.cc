#include "common/http/filter/transformer.h"

#include "test/mocks/common.h"
#include "test/mocks/server/mocks.h"
#include "test/mocks/upstream/mocks.h"
#include "test/test_common/utility.h"

#include "fmt/format.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::AtLeast;
using testing::Invoke;
using testing::Return;
using testing::ReturnPointee;
using testing::ReturnRef;
using testing::SaveArg;
using testing::WithArg;
using testing::_;

using json = nlohmann::json;

namespace Envoy {
namespace Http {

TEST(TransformerInstance, ReplacesValueFromContext) {
  json originalbody;
  originalbody["field1"] = "value1";
  Envoy::Http::TestHeaderMapImpl headers;
  TransformerInstance t(headers, {}, originalbody);

  auto res = t.render("{{field1}}");

  EXPECT_EQ(originalbody["field1"], res);
}

TEST(TransformerInstance, ReplacesValueFromInlineHeader) {
  json originalbody;
  originalbody["field1"] = "value1";
  std::string path = "/getsomething";

  Envoy::Http::TestHeaderMapImpl headers{
      {":method", "GET"}, {":authority", "www.solo.io"}, {":path", path}};

  TransformerInstance t(headers, {}, originalbody);

  auto res = t.render("{{header(\":path\")}}");

  EXPECT_EQ(path, res);
}

TEST(TransformerInstance, ReplacesValueFromCustomHeader) {
  json originalbody;
  originalbody["field1"] = "value1";
  std::string header = "blah blah";
  Envoy::Http::TestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"},
                                         {"x-custom-header", header}};
  TransformerInstance t(headers, {}, originalbody);

  auto res = t.render("{{header(\"x-custom-header\")}}");

  EXPECT_EQ(header, res);
}

TEST(TransformerInstance, ReplaceFromExtracted) {
  json originalbody;
  std::map<std::string, std::string> extractions;
  std::string field = "res";
  extractions["f"] = field;
  Envoy::Http::TestHeaderMapImpl headers;
  TransformerInstance t(headers, extractions, originalbody);

  auto res = t.render("{{extraction(\"f\")}}");

  EXPECT_EQ(field, res);
}

TEST(TransformerInstance, ReplaceFromNonExistentExtraction) {
  json originalbody;
  std::map<std::string, std::string> extractions;
  extractions["foo"] = "bar";
  Envoy::Http::TestHeaderMapImpl headers;
  TransformerInstance t(headers, extractions, originalbody);

  auto res = t.render("{{extraction(\"notsuchfield\")}}");

  EXPECT_EQ("", res);
}

TEST(ExtractorUtil, ExtractIdFromHeader) {
  Envoy::Http::TestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/users/123"}};
  envoy::api::v2::filter::http::Extraction extractor;
  extractor.set_header(":path");
  extractor.set_regex("/users/(\\d+)");
  extractor.set_subgroup(1);
  auto res = ExtractorUtil::extract(extractor, headers);

  EXPECT_EQ("123", res);
}

} // namespace Http
} // namespace Envoy
