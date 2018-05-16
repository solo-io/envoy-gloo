#include "common/http/filter/transformer.h"

#include "test/mocks/common.h"
#include "test/mocks/server/mocks.h"
#include "test/mocks/upstream/mocks.h"
#include "test/test_common/utility.h"

#include "fmt/format.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::AtLeast;
using testing::HasSubstr;
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
  TestHeaderMapImpl headers;
  TransformerInstance t(headers, {}, originalbody);

  auto res = t.render("{{field1}}");

  EXPECT_EQ(originalbody["field1"], res);
}

TEST(TransformerInstance, ReplacesValueFromInlineHeader) {
  json originalbody;
  originalbody["field1"] = "value1";
  std::string path = "/getsomething";

  TestHeaderMapImpl headers{
      {":method", "GET"}, {":authority", "www.solo.io"}, {":path", path}};

  TransformerInstance t(headers, {}, originalbody);

  auto res = t.render("{{header(\":path\")}}");

  EXPECT_EQ(path, res);
}

TEST(TransformerInstance, ReplacesValueFromCustomHeader) {
  json originalbody;
  originalbody["field1"] = "value1";
  std::string header = "blah blah";
  TestHeaderMapImpl headers{{":method", "GET"},
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
  TestHeaderMapImpl headers;
  TransformerInstance t(headers, extractions, originalbody);

  auto res = t.render("{{extraction(\"f\")}}");

  EXPECT_EQ(field, res);
}

TEST(TransformerInstance, ReplaceFromNonExistentExtraction) {
  json originalbody;
  std::map<std::string, std::string> extractions;
  extractions["foo"] = "bar";
  TestHeaderMapImpl headers;
  TransformerInstance t(headers, extractions, originalbody);

  auto res = t.render("{{extraction(\"notsuchfield\")}}");

  EXPECT_EQ("", res);
}

TEST(ExtractorUtil, ExtractIdFromHeader) {
  TestHeaderMapImpl headers{{":method", "GET"},
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
  TestHeaderMapImpl headers{{":method", "GET"},
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

  Transformer transformer(transformation);
  transformer.transform(headers, body);

  std::string res = TestUtility::bufferToString(body);

  EXPECT_EQ("123456789", res);
  EXPECT_EQ("ABC", headers.get_("x-header"));
}

TEST(Transformer, transformSimple) {
  TestHeaderMapImpl headers{{":method", "GET"},
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

  Transformer transformer(transformation);
  transformer.transform(headers, body);

  std::string res = TestUtility::bufferToString(body);

  EXPECT_EQ("123456789", res);
  EXPECT_EQ("ABC", headers.get_("x-header"));
}

TEST(Transformer, transformSimpleNestedStructs) {
  TestHeaderMapImpl headers{{":method", "GET"},
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

  Transformer transformer(transformation);
  transformer.transform(headers, body);

  std::string res = TestUtility::bufferToString(body);

  EXPECT_EQ("123456789", res);
  EXPECT_EQ("ABC", headers.get_("x-header"));
}

TEST(Transformer, transformPassthrough) {
  TestHeaderMapImpl headers{{":method", "GET"},
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

  Transformer transformer(transformation);
  transformer.transform(headers, body);

  std::string res = TestUtility::bufferToString(body);

  EXPECT_EQ(emptyBody, res);
  EXPECT_EQ("default", headers.get_("x-header"));
}

TEST(Transformer, transformMergeExtractorsToBody) {
  TestHeaderMapImpl headers{{":method", "GET"},
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

  Transformer transformer(transformation);
  transformer.transform(headers, body);

  std::string res = TestUtility::bufferToString(body);

  EXPECT_EQ("{\"ext1\":\"123\"}", res);
}

TEST(Transformer, transformBodyNotSet) {
  TestHeaderMapImpl headers{{":method", "GET"},
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

  Transformer transformer(transformation);
  transformer.transform(headers, body);

  std::string res = TestUtility::bufferToString(body);

  EXPECT_EQ(originalBody, res);
  EXPECT_EQ("456", headers.get_("x-header"));
}

TEST(Transformer, transformWithHyphens) {
  TestHeaderMapImpl headers{
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

  Transformer transformer(transformation);
  transformer.transform(headers, body);

  std::string res = TestUtility::bufferToString(body);

  EXPECT_THAT(res, HasSubstr("\"764b.0f_0f-7319-4b29-bbd0-887a39705a70\""));
}

} // namespace Http
} // namespace Envoy
