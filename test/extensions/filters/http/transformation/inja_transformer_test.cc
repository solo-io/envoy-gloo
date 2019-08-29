#include "extensions/filters/http/transformation/inja_transformer.h"

#include "test/mocks/common.h"
#include "test/mocks/server/mocks.h"
#include "test/mocks/upstream/mocks.h"

#include "fmt/format.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::_;
using testing::AtLeast;
using testing::HasSubstr;
using testing::Invoke;
using testing::Return;
using testing::ReturnPointee;
using testing::ReturnRef;
using testing::SaveArg;
using testing::WithArg;

using json = nlohmann::json;

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

TEST(TransformerInstance, ReplacesValueFromContext) {
  json originalbody;
  originalbody["field1"] = "value1";
  Http::TestHeaderMapImpl headers;
  TransformerInstance t(headers, {}, originalbody);

  auto res = t.render("{{field1}}");

  EXPECT_EQ(originalbody["field1"], res);
}

TEST(TransformerInstance, ReplacesValueFromInlineHeader) {
  json originalbody;
  originalbody["field1"] = "value1";
  std::string path = "/getsomething";

  Http::TestHeaderMapImpl headers{
      {":method", "GET"}, {":authority", "www.solo.io"}, {":path", path}};

  TransformerInstance t(headers, {}, originalbody);

  auto res = t.render("{{header(\":path\")}}");

  EXPECT_EQ(path, res);
}

TEST(TransformerInstance, ReplacesValueFromCustomHeader) {
  json originalbody;
  originalbody["field1"] = "value1";
  std::string header = "blah blah";
  Http::TestHeaderMapImpl headers{{":method", "GET"},
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
  Http::TestHeaderMapImpl headers;
  TransformerInstance t(headers, extractions, originalbody);

  auto res = t.render("{{extraction(\"f\")}}");

  EXPECT_EQ(field, res);
}

TEST(TransformerInstance, ReplaceFromNonExistentExtraction) {
  json originalbody;
  std::map<std::string, std::string> extractions;
  extractions["foo"] = "bar";
  Http::TestHeaderMapImpl headers;
  TransformerInstance t(headers, extractions, originalbody);

  auto res = t.render("{{extraction(\"notsuchfield\")}}");

  EXPECT_EQ("", res);
}

TEST(ExtractorUtil, ExtractIdFromHeader) {
  Http::TestHeaderMapImpl headers{{":method", "GET"},
                                  {":authority", "www.solo.io"},
                                  {":path", "/users/123"}};
  envoy::api::v2::filter::http::Extraction extractor;
  extractor.set_header(":path");
  extractor.set_regex("/users/(\\d+)");
  extractor.set_subgroup(1);
  auto res = ExtractorUtil::extract(extractor, headers);

  EXPECT_EQ("123", res);
}

TEST(Transformer, transform) {
  Http::TestHeaderMapImpl headers{{":method", "GET"},
                                  {":authority", "www.solo.io"},
                                  {"x-test", "789"},
                                  {":path", "/users/123"}};
  Buffer::OwnedImpl body("{\"a\":\"456\"}");

  envoy::api::v2::filter::http::Extraction extractor;
  extractor.set_header(":path");
  extractor.set_regex("/users/(\\d+)");
  extractor.set_subgroup(1);

  envoy::api::v2::filter::http::TransformationTemplate transformation;

  (*transformation.mutable_extractors())["ext1"] = extractor;
  transformation.mutable_body()->set_text(
      "{{extraction(\"ext1\")}}{{a}}{{header(\"x-test\")}}");

  (*transformation.mutable_headers())["x-header"].set_text(
      "{{upper(\"abc\")}}");
  transformation.set_advanced_templates(true);

  InjaTransformer transformer(transformation);
  transformer.transform(headers, body);

  std::string res = body.toString();

  EXPECT_EQ("123456789", res);
  EXPECT_EQ("ABC", headers.get_("x-header"));
}

TEST(Transformer, transformSimple) {
  Http::TestHeaderMapImpl headers{{":method", "GET"},
                                  {":authority", "www.solo.io"},
                                  {"x-test", "789"},
                                  {":path", "/users/123"}};
  Buffer::OwnedImpl body("{\"a\":\"456\"}");

  envoy::api::v2::filter::http::Extraction extractor;
  extractor.set_header(":path");
  extractor.set_regex("/users/(\\d+)");
  extractor.set_subgroup(1);

  envoy::api::v2::filter::http::TransformationTemplate transformation;

  (*transformation.mutable_extractors())["ext1"] = extractor;
  transformation.mutable_body()->set_text(
      "{{ext1}}{{a}}{{header(\"x-test\")}}");

  (*transformation.mutable_headers())["x-header"].set_text(
      "{{upper(\"abc\")}}");
  transformation.set_advanced_templates(false);

  InjaTransformer transformer(transformation);
  transformer.transform(headers, body);

  std::string res = body.toString();

  EXPECT_EQ("123456789", res);
  EXPECT_EQ("ABC", headers.get_("x-header"));
}

TEST(Transformer, transformSimpleNestedStructs) {
  Http::TestHeaderMapImpl headers{{":method", "GET"},
                                  {":authority", "www.solo.io"},
                                  {"x-test", "789"},
                                  {":path", "/users/123"}};
  Buffer::OwnedImpl body("{\"a\":\"456\"}");

  envoy::api::v2::filter::http::Extraction extractor;
  extractor.set_header(":path");
  extractor.set_regex("/users/(\\d+)");
  extractor.set_subgroup(1);

  envoy::api::v2::filter::http::TransformationTemplate transformation;

  (*transformation.mutable_extractors())["ext1.field1"] = extractor;
  transformation.mutable_body()->set_text(
      "{{ext1.field1}}{{a}}{{header(\"x-test\")}}");

  (*transformation.mutable_headers())["x-header"].set_text(
      "{{upper(\"abc\")}}");
  transformation.set_advanced_templates(false);

  InjaTransformer transformer(transformation);
  transformer.transform(headers, body);

  std::string res = body.toString();

  EXPECT_EQ("123456789", res);
  EXPECT_EQ("ABC", headers.get_("x-header"));
}

TEST(Transformer, transformPassthrough) {
  Http::TestHeaderMapImpl headers{{":method", "GET"},
                                  {":authority", "www.solo.io"},
                                  {"x-test", "789"},
                                  {":path", "/users/123"}};
  // in passthrough mode the filter gives us an empty body
  std::string emptyBody = "";
  Buffer::OwnedImpl body(emptyBody);

  envoy::api::v2::filter::http::TransformationTemplate transformation;

  transformation.mutable_passthrough();
  (*transformation.mutable_headers())["x-header"].set_text(
      "{{default(a,\"default\")}}");

  transformation.set_advanced_templates(true);

  InjaTransformer transformer(transformation);
  transformer.transform(headers, body);

  std::string res = body.toString();

  EXPECT_EQ(emptyBody, res);
  EXPECT_EQ("default", headers.get_("x-header"));
}

TEST(Transformer, transformMergeExtractorsToBody) {
  Http::TestHeaderMapImpl headers{{":method", "GET"},
                                  {":authority", "www.solo.io"},
                                  {"x-test", "789"},
                                  {":path", "/users/123"}};
  // in passthrough mode the filter gives us an empty body
  std::string emptyBody = "";
  Buffer::OwnedImpl body(emptyBody);

  envoy::api::v2::filter::http::TransformationTemplate transformation;

  transformation.mutable_merge_extractors_to_body();

  envoy::api::v2::filter::http::Extraction extractor;
  extractor.set_header(":path");
  extractor.set_regex("/users/(\\d+)");
  extractor.set_subgroup(1);
  (*transformation.mutable_extractors())["ext1"] = extractor;

  transformation.set_advanced_templates(false);

  InjaTransformer transformer(transformation);
  transformer.transform(headers, body);

  std::string res = body.toString();

  EXPECT_EQ("{\"ext1\":\"123\"}", res);
}

TEST(Transformer, transformBodyNotSet) {
  Http::TestHeaderMapImpl headers{{":method", "GET"},
                                  {":authority", "www.solo.io"},
                                  {"x-test", "789"},
                                  {":path", "/users/123"}};
  std::string originalBody = "{\"a\":\"456\"}";
  Buffer::OwnedImpl body(originalBody);

  envoy::api::v2::filter::http::TransformationTemplate transformation;

  // trying to get a value from the body; which should be available in default
  // mode
  (*transformation.mutable_headers())["x-header"].set_text("{{a}}");

  transformation.set_advanced_templates(true);

  InjaTransformer transformer(transformation);
  transformer.transform(headers, body);

  std::string res = body.toString();

  EXPECT_EQ(originalBody, res);
  EXPECT_EQ("456", headers.get_("x-header"));
}

TEST(Transformer, transformWithHyphens) {
  Http::TestHeaderMapImpl headers{
      {":method", "GET"},
      {":path", "/accounts/764b.0f_0f-7319-4b29-bbd0-887a39705a70"}};
  Buffer::OwnedImpl body("{}");

  envoy::api::v2::filter::http::Extraction extractor;
  extractor.set_header(":path");
  extractor.set_regex("/accounts/([\\-._[:alnum:]]+)");
  extractor.set_subgroup(1);

  envoy::api::v2::filter::http::TransformationTemplate transformation;

  (*transformation.mutable_extractors())["id"] = extractor;

  transformation.set_advanced_templates(false);
  transformation.mutable_merge_extractors_to_body();

  InjaTransformer transformer(transformation);
  transformer.transform(headers, body);

  std::string res = body.toString();

  EXPECT_THAT(res, HasSubstr("\"764b.0f_0f-7319-4b29-bbd0-887a39705a70\""));
}

TEST(ExtractorUtil, RemoveHeadersUsingEmptyTemplate) {
  const std::string content_type = "content-type";
  Http::TestHeaderMapImpl headers{
      {":method", "GET"}, {":path", "/foo"}, {content_type, "x-test"}};
  Buffer::OwnedImpl body("{}");

  envoy::api::v2::filter::http::TransformationTemplate transformation;
  envoy::api::v2::filter::http::InjaTemplate empty;

  (*transformation.mutable_headers())[content_type] = empty;

  InjaTransformer transformer(transformation);

  EXPECT_TRUE(headers.has(content_type));
  transformer.transform(headers, body);
  EXPECT_FALSE(headers.has(content_type));
}

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
