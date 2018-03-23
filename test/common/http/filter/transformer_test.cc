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

TEST(Transformer, transform) {
  Envoy::Http::TestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {"x-test", "789"},
                                         {":path", "/users/123"}};
  Buffer::OwnedImpl body("{\"a\":\"456\"}");

  envoy::api::v2::filter::http::Extraction extractor;
  extractor.set_header(":path");
  extractor.set_regex("/users/(\\d+)");
  extractor.set_subgroup(1);

  envoy::api::v2::filter::http::Transformation transformation;

  (*transformation.mutable_extractors())["ext1"] = extractor;
  transformation.mutable_transformation_template()->mutable_body()->set_text(
      "{{extraction(\"ext1\")}}{{a}}{{header(\"x-test\")}}");

  (*transformation.mutable_transformation_template()
        ->mutable_headers())["x-header"]
      .set_text("{{upper(\"abc\")}}");

  Transformer transformer(transformation, true);
  transformer.transform(headers, body);

  std::string res = TestUtility::bufferToString(body);

  EXPECT_EQ("123456789", res);
  EXPECT_EQ("ABC", headers.get_("x-header"));
}

TEST(Transformer, transformSimple) {
  Envoy::Http::TestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {"x-test", "789"},
                                         {":path", "/users/123"}};
  Buffer::OwnedImpl body("{\"a\":\"456\"}");

  envoy::api::v2::filter::http::Extraction extractor;
  extractor.set_header(":path");
  extractor.set_regex("/users/(\\d+)");
  extractor.set_subgroup(1);

  envoy::api::v2::filter::http::Transformation transformation;

  (*transformation.mutable_extractors())["ext1"] = extractor;
  transformation.mutable_transformation_template()->mutable_body()->set_text(
      "{{ext1}}{{a}}{{header(\"x-test\")}}");

  (*transformation.mutable_transformation_template()
        ->mutable_headers())["x-header"]
      .set_text("{{upper(\"abc\")}}");

  Transformer transformer(transformation, false);
  transformer.transform(headers, body);

  std::string res = TestUtility::bufferToString(body);

  EXPECT_EQ("123456789", res);
  EXPECT_EQ("ABC", headers.get_("x-header"));
}

TEST(Transformer, transformSimpleNestedStructs) {
  Envoy::Http::TestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {"x-test", "789"},
                                         {":path", "/users/123"}};
  Buffer::OwnedImpl body("{\"a\":\"456\"}");

  envoy::api::v2::filter::http::Extraction extractor;
  extractor.set_header(":path");
  extractor.set_regex("/users/(\\d+)");
  extractor.set_subgroup(1);

  envoy::api::v2::filter::http::Transformation transformation;

  (*transformation.mutable_extractors())["ext1.field1"] = extractor;
  transformation.mutable_transformation_template()->mutable_body()->set_text(
      "{{ext1.field1}}{{a}}{{header(\"x-test\")}}");

  (*transformation.mutable_transformation_template()
        ->mutable_headers())["x-header"]
      .set_text("{{upper(\"abc\")}}");

  Transformer transformer(transformation, false);
  transformer.transform(headers, body);

  std::string res = TestUtility::bufferToString(body);

  EXPECT_EQ("123456789", res);
  EXPECT_EQ("ABC", headers.get_("x-header"));
}

TEST(Transformer, transformPassthrough) {
  Envoy::Http::TestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {"x-test", "789"},
                                         {":path", "/users/123"}};
  // in passthrough mode the filter gives us an empty body
  std::string emptyBody = "";
  Buffer::OwnedImpl body(emptyBody);

  envoy::api::v2::filter::http::Transformation transformation;

  transformation.mutable_transformation_template()->mutable_passthrough();
  (*transformation.mutable_transformation_template()
        ->mutable_headers())["x-header"]
      .set_text("{{default(a,\"default\")}}");

  Transformer transformer(transformation, true);
  transformer.transform(headers, body);

  std::string res = TestUtility::bufferToString(body);

  EXPECT_EQ(emptyBody, res);
  EXPECT_EQ("default", headers.get_("x-header"));
}

TEST(Transformer, transformMergeExtractorsToBody) {
  Envoy::Http::TestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {"x-test", "789"},
                                         {":path", "/users/123"}};
  // in passthrough mode the filter gives us an empty body
  std::string emptyBody = "";
  Buffer::OwnedImpl body(emptyBody);

  envoy::api::v2::filter::http::Transformation transformation;

  transformation.mutable_transformation_template()
      ->mutable_merge_extractors_to_body();

  envoy::api::v2::filter::http::Extraction extractor;
  extractor.set_header(":path");
  extractor.set_regex("/users/(\\d+)");
  extractor.set_subgroup(1);
  (*transformation.mutable_extractors())["ext1"] = extractor;

  Transformer transformer(transformation, false);
  transformer.transform(headers, body);

  std::string res = TestUtility::bufferToString(body);

  EXPECT_EQ("{\"ext1\":\"123\"}", res);
}

TEST(Transformer, transformBodyNotSet) {
  Envoy::Http::TestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {"x-test", "789"},
                                         {":path", "/users/123"}};
  std::string originalBody = "{\"a\":\"456\"}";
  Buffer::OwnedImpl body(originalBody);

  envoy::api::v2::filter::http::Transformation transformation;

  // trying to get a value from the body; which should be available in default
  // mode
  (*transformation.mutable_transformation_template()
        ->mutable_headers())["x-header"]
      .set_text("{{a}}");

  Transformer transformer(transformation, true);
  transformer.transform(headers, body);

  std::string res = TestUtility::bufferToString(body);

  EXPECT_EQ(originalBody, res);
  EXPECT_EQ("456", headers.get_("x-header"));
}

} // namespace Http
} // namespace Envoy
