#include "extensions/filters/http/transformation/inja_transformer.h"

#include "test/mocks/common.h"
#include "test/mocks/http/mocks.h"
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

using TransformationTemplate =
    envoy::api::v2::filter::http::TransformationTemplate;

namespace {
std::function<const std::string &()> empty_body = [] { return EMPTY_STRING; };
}

inja::Template parse(std::string s) {
  inja::ParserConfig parser_config;
  inja::LexerConfig lexer_config;
  inja::TemplateStorage template_storage;

  inja::Parser parser(parser_config, lexer_config, template_storage);
  return parser.parse(s);
}

TEST(TransformerInstance, ReplacesValueFromContext) {
  json originalbody;
  originalbody["field1"] = "value1";
  Http::TestHeaderMapImpl headers;
  
  TransformerInstance t(headers, empty_body, {}, originalbody);

  auto res = t.render(parse("{{field1}}"));

  EXPECT_EQ(originalbody["field1"], res);
}

TEST(TransformerInstance, ReplacesValueFromInlineHeader) {
  json originalbody;
  originalbody["field1"] = "value1";
  std::string path = "/getsomething";

  Http::TestHeaderMapImpl headers{
      {":method", "GET"}, {":authority", "www.solo.io"}, {":path", path}};

  TransformerInstance t(headers, empty_body, {}, originalbody);

  auto res = t.render(parse("{{header(\":path\")}}"));

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
                                  
  TransformerInstance t(headers, empty_body, {}, originalbody);

  auto res = t.render(parse("{{header(\"x-custom-header\")}}"));

  EXPECT_EQ(header, res);
}

TEST(TransformerInstance, ReplaceFromExtracted) {
  json originalbody;
  std::unordered_map<std::string, absl::string_view> extractions;
  absl::string_view field = "res";
  extractions["f"] = field;
  Http::TestHeaderMapImpl headers;
  
  TransformerInstance t(headers, empty_body, extractions, originalbody);

  auto res = t.render(parse("{{extraction(\"f\")}}"));

  EXPECT_EQ(field, res);
}

TEST(TransformerInstance, ReplaceFromNonExistentExtraction) {
  json originalbody;
  std::unordered_map<std::string, absl::string_view> extractions;
  extractions["foo"] = absl::string_view("bar");
  Http::TestHeaderMapImpl headers;
  
  TransformerInstance t(headers, empty_body, extractions, originalbody);

  auto res = t.render(parse("{{extraction(\"notsuchfield\")}}"));

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
  std::string res(Extractor(extractor).extract(headers, empty_body));

  EXPECT_EQ("123", res);
}

TEST(ExtractorUtil, ExtractorFail) {
  Http::TestHeaderMapImpl headers{{":method", "GET"},
                                  {":authority", "www.solo.io"},
                                  {":path", "/users/123"}};
  envoy::api::v2::filter::http::Extraction extractor;
  extractor.set_header(":path");
  extractor.set_regex("ILLEGAL REGEX \\ \\ \\ \\ a\\ \\a\\ a\\  \\d+)");
  extractor.set_subgroup(1);
  EXPECT_THROW_WITH_MESSAGE(Extractor a(extractor), EnvoyException,
                            "Invalid regex 'ILLEGAL REGEX \\ \\ \\ \\ a\\ "
                            "\\a\\ a\\  \\d+)': regex_error");
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

  TransformationTemplate transformation;

  (*transformation.mutable_extractors())["ext1"] = extractor;
  transformation.mutable_body()->set_text(
      "{{extraction(\"ext1\")}}{{a}}{{header(\"x-test\")}}");

  (*transformation.mutable_headers())["x-header"].set_text(
      "{{upper(\"abc\")}}");
  transformation.set_advanced_templates(true);

  InjaTransformer transformer(transformation);
  NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_{};
  transformer.transform(headers, body, filter_callbacks_);

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

  TransformationTemplate transformation;

  (*transformation.mutable_extractors())["ext1"] = extractor;
  transformation.mutable_body()->set_text(
      "{{ext1}}{{a}}{{header(\"x-test\")}}");

  (*transformation.mutable_headers())["x-header"].set_text(
      "{{upper(\"abc\")}}");
  transformation.set_advanced_templates(false);

  InjaTransformer transformer(transformation);
  NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_{};
  transformer.transform(headers, body, filter_callbacks_);

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

  TransformationTemplate transformation;

  (*transformation.mutable_extractors())["ext1.field1"] = extractor;
  transformation.mutable_body()->set_text(
      "{{ext1.field1}}{{a}}{{header(\"x-test\")}}");

  (*transformation.mutable_headers())["x-header"].set_text(
      "{{upper(\"abc\")}}");
  transformation.set_advanced_templates(false);

  InjaTransformer transformer(transformation);
  NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_{};
  transformer.transform(headers, body, filter_callbacks_);

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

  TransformationTemplate transformation;

  transformation.mutable_passthrough();
  (*transformation.mutable_headers())["x-header"].set_text(
      "{{default(a,\"default\")}}");

  transformation.set_advanced_templates(true);

  InjaTransformer transformer(transformation);
  NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_{};
  transformer.transform(headers, body, filter_callbacks_);

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

  TransformationTemplate transformation;

  transformation.mutable_merge_extractors_to_body();

  envoy::api::v2::filter::http::Extraction extractor;
  extractor.set_header(":path");
  extractor.set_regex("/users/(\\d+)");
  extractor.set_subgroup(1);
  (*transformation.mutable_extractors())["ext1"] = extractor;

  transformation.set_advanced_templates(false);

  InjaTransformer transformer(transformation);
  NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_{};
  transformer.transform(headers, body, filter_callbacks_);

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

  TransformationTemplate transformation;

  // trying to get a value from the body; which should be available in default
  // mode
  (*transformation.mutable_headers())["x-header"].set_text("{{a}}");

  transformation.set_advanced_templates(true);

  InjaTransformer transformer(transformation);
  NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_{};
  transformer.transform(headers, body, filter_callbacks_);

  std::string res = body.toString();

  EXPECT_EQ(originalBody, res);
  EXPECT_EQ("456", headers.get_("x-header"));
}

TEST(InjaTransformer, transformWithHyphens) {
  Http::TestHeaderMapImpl headers{
      {":method", "GET"},
      {":path", "/accounts/764b.0f_0f-7319-4b29-bbd0-887a39705a70"}};
  Buffer::OwnedImpl body("{}");

  envoy::api::v2::filter::http::Extraction extractor;
  extractor.set_header(":path");
  extractor.set_regex("/accounts/([\\-._[:alnum:]]+)");
  extractor.set_subgroup(1);

  TransformationTemplate transformation;

  (*transformation.mutable_extractors())["id"] = extractor;

  transformation.set_advanced_templates(false);
  transformation.mutable_merge_extractors_to_body();

  InjaTransformer transformer(transformation);
  NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_{};
  transformer.transform(headers, body, filter_callbacks_);

  std::string res = body.toString();

  EXPECT_THAT(res, HasSubstr("\"764b.0f_0f-7319-4b29-bbd0-887a39705a70\""));
}

TEST(InjaTransformer, RemoveHeadersUsingEmptyTemplate) {
  const std::string content_type = "content-type";
  Http::TestHeaderMapImpl headers{
      {":method", "GET"}, {":path", "/foo"}, {content_type, "x-test"}};
  Buffer::OwnedImpl body("{}");

  TransformationTemplate transformation;
  envoy::api::v2::filter::http::InjaTemplate empty;

  (*transformation.mutable_headers())[content_type] = empty;

  InjaTransformer transformer(transformation);

  EXPECT_TRUE(headers.has(content_type));
  NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_{};
  transformer.transform(headers, body, filter_callbacks_);
  EXPECT_FALSE(headers.has(content_type));
}

TEST(InjaTransformer, DontParseBodyAndExtractFromIt) {
  const std::string content_type = "content-type";
  Http::TestHeaderMapImpl headers{
      {":method", "GET"}, {":path", "/foo"}, {content_type, "x-test"}};
  Buffer::OwnedImpl body("not json body");

  TransformationTemplate transformation;
  transformation.set_parse_body_behavior(TransformationTemplate::DontParse);
  transformation.set_advanced_templates(true);

  envoy::api::v2::filter::http::Extraction extractor;
  extractor.mutable_body();
  extractor.set_regex("not ([\\-._[:alnum:]]+) body");
  extractor.set_subgroup(1);
  (*transformation.mutable_extractors())["param"] = extractor;

  transformation.mutable_body()->set_text("{{extraction(\"param\")}}");

  InjaTransformer transformer(transformation);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_{};
  transformer.transform(headers, body, filter_callbacks_);
  EXPECT_EQ(body.toString(), "json");
}

TEST(InjaTransformer, UseBodyFunction) {
  const std::string content_type = "content-type";
  Http::TestHeaderMapImpl headers{
      {":method", "GET"}, {":path", "/foo"}, {content_type, "x-test"}};
  TransformationTemplate transformation;
  transformation.set_parse_body_behavior(TransformationTemplate::DontParse);
  transformation.set_advanced_templates(true);

  transformation.mutable_body()->set_text("{{body()}} {{body()}}");

  InjaTransformer transformer(transformation);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_{};
  Buffer::OwnedImpl body("1");
  transformer.transform(headers, body, filter_callbacks_);
  EXPECT_EQ(body.toString(), "1 1");
}

TEST(InjaTransformer, UseDynamicMeta) {
  const std::string content_type = "content-type";
  Http::TestHeaderMapImpl headers{
      {":method", "GET"}, {":path", "/foo"}, {content_type, "x-test"}};
  TransformationTemplate transformation;
  transformation.set_parse_body_behavior(TransformationTemplate::DontParse);
  transformation.set_advanced_templates(true);

  auto dynamic_meta = transformation.add_dynamic_metadata_values();
  dynamic_meta->set_key("foo");
  dynamic_meta->mutable_value()->set_text("{{body()}}");
  dynamic_meta = transformation.add_dynamic_metadata_values();
  dynamic_meta->set_key("bar");
  dynamic_meta->mutable_value()->set_text("123");

  InjaTransformer transformer(transformation);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> filter_callbacks_{};

  EXPECT_CALL(filter_callbacks_.stream_info_, setDynamicMetadata("io.solo.transformation", _)).Times(2);
  Buffer::OwnedImpl body("1");
  transformer.transform(headers, body, filter_callbacks_);
  EXPECT_EQ(body.toString(), "1");
}

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
