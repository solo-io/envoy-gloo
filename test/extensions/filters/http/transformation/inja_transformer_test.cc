#include "source/extensions/filters/http/solo_well_known_names.h"
#include "source/extensions/filters/http/transformation/inja_transformer.h"
#include "source/common/common/base64.h"

#include "test/mocks/common.h"
#include "test/mocks/http/mocks.h"
#include "test/mocks/server/mocks.h"
#include "test/mocks/upstream/mocks.h"
#include "test/test_common/environment.h"

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
  Http::TestRequestHeaderMapImpl headers;
  std::unordered_map<std::string, absl::string_view> extractions;
  std::unordered_map<std::string, std::string> env;
  envoy::config::core::v3::Metadata *cluster_metadata{};

  TransformerInstance t(headers, &headers, empty_body, extractions,
                        originalbody, env, cluster_metadata);

  auto res = t.render(parse("{{field1}}"));

  EXPECT_EQ(originalbody["field1"], res);
}

TEST(TransformerInstance, ReplacesValueFromInlineHeader) {
  json originalbody;
  originalbody["field1"] = "value1";
  std::string path = "/getsomething";

  Http::TestRequestHeaderMapImpl headers{
      {":method", "GET"}, {":authority", "www.solo.io"}, {":path", path}};
  std::unordered_map<std::string, absl::string_view> extractions;
  std::unordered_map<std::string, std::string> env;
  envoy::config::core::v3::Metadata *cluster_metadata{};

  TransformerInstance t(headers, &headers, empty_body, extractions,
                        originalbody, env, cluster_metadata);

  auto res = t.render(parse("{{header(\":path\")}}"));

  EXPECT_EQ(path, res);
}

TEST(TransformerInstance, ReplacesValueFromCustomHeader) {
  json originalbody;
  originalbody["field1"] = "value1";
  std::string header = "blah blah";
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"},
                                         {"x-custom-header", header}};
  std::unordered_map<std::string, absl::string_view> extractions;
  std::unordered_map<std::string, std::string> env;
  envoy::config::core::v3::Metadata *cluster_metadata{};

  TransformerInstance t(headers, &headers, empty_body, extractions,
                        originalbody, env, cluster_metadata);

  auto res = t.render(parse("{{header(\"x-custom-header\")}}"));

  EXPECT_EQ(header, res);
}

TEST(TransformerInstance, ReplaceFromExtracted) {
  json originalbody;
  std::unordered_map<std::string, absl::string_view> extractions;
  absl::string_view field = "res";
  extractions["f"] = field;
  Http::TestRequestHeaderMapImpl headers;
  std::unordered_map<std::string, std::string> env;
  envoy::config::core::v3::Metadata *cluster_metadata{};

  TransformerInstance t(headers, &headers, empty_body, extractions,
                        originalbody, env, cluster_metadata);

  auto res = t.render(parse("{{extraction(\"f\")}}"));

  EXPECT_EQ(field, res);
}

TEST(TransformerInstance, ReplaceFromNonExistentExtraction) {
  json originalbody;
  std::unordered_map<std::string, absl::string_view> extractions;
  extractions["foo"] = absl::string_view("bar");
  Http::TestRequestHeaderMapImpl headers;
  std::unordered_map<std::string, std::string> env;
  envoy::config::core::v3::Metadata *cluster_metadata{};

  TransformerInstance t(headers, &headers, empty_body, extractions,
                        originalbody, env, cluster_metadata);

  auto res = t.render(parse("{{extraction(\"notsuchfield\")}}"));

  EXPECT_EQ("", res);
}

TEST(TransformerInstance, Environment) {
  json originalbody;
  std::unordered_map<std::string, absl::string_view> extractions;
  Http::TestRequestHeaderMapImpl headers;
  std::unordered_map<std::string, std::string> env;
  envoy::config::core::v3::Metadata *cluster_metadata{};
  env["FOO"] = "BAR";

  TransformerInstance t(headers, &headers, empty_body, extractions,
                        originalbody, env, cluster_metadata);

  auto res = t.render(parse("{{env(\"FOO\")}}"));
  EXPECT_EQ("BAR", res);
}

TEST(TransformerInstance, EmptyEnvironment) {
  json originalbody;
  std::unordered_map<std::string, absl::string_view> extractions;
  Http::TestRequestHeaderMapImpl headers;

  std::unordered_map<std::string, std::string> env;
  envoy::config::core::v3::Metadata *cluster_metadata{};
  TransformerInstance t(headers, &headers, empty_body, extractions,
                        originalbody, env, cluster_metadata);

  auto res = t.render(parse("{{env(\"FOO\")}}"));
  EXPECT_EQ("", res);
}

TEST(TransformerInstance, ClusterMetadata) {
  json originalbody;
  std::unordered_map<std::string, absl::string_view> extractions;
  Http::TestRequestHeaderMapImpl headers;

  std::unordered_map<std::string, std::string> env;

  envoy::config::core::v3::Metadata cluster_metadata;
  cluster_metadata.mutable_filter_metadata()->insert(
      {SoloHttpFilterNames::get().Transformation,
       MessageUtil::keyValueStruct("io.solo.hostname", "foo.example.com")});

  TransformerInstance t(headers, &headers, empty_body, extractions,
                        originalbody, env, &cluster_metadata);

  auto res = t.render(parse("{{clusterMetadata(\"io.solo.hostname\")}}"));
  EXPECT_EQ("foo.example.com", res);
}

TEST(TransformerInstance, EmptyClusterMetadata) {
  json originalbody;
  std::unordered_map<std::string, absl::string_view> extractions;
  Http::TestRequestHeaderMapImpl headers;

  std::unordered_map<std::string, std::string> env;
  envoy::config::core::v3::Metadata *cluster_metadata{};

  TransformerInstance t(headers, &headers, empty_body, extractions,
                        originalbody, env, cluster_metadata);

  auto res = t.render(parse("{{clusterMetadata(\"io.solo.hostname\")}}"));
  EXPECT_EQ("", res);
}

TEST(TransformerInstance, RequestHeaders) {
  json originalbody;
  std::unordered_map<std::string, absl::string_view> extractions;
  Http::TestResponseHeaderMapImpl response_headers{{":status", "200"}};
  Http::TestRequestHeaderMapImpl request_headers{{":method", "GET"}};

  std::unordered_map<std::string, std::string> env;
  envoy::config::core::v3::Metadata *cluster_metadata{};

  TransformerInstance t(response_headers, &request_headers, empty_body,
                        extractions, originalbody, env, cluster_metadata);

  auto res = t.render(
      parse("{{header(\":status\")}}-{{request_header(\":method\")}}"));
  EXPECT_EQ("200-GET", res);
}

TEST(Extraction, ExtractIdFromHeader) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/users/123"}};
  envoy::api::v2::filter::http::Extraction extractor;
  extractor.set_header(":path");
  extractor.set_regex("/users/(\\d+)");
  extractor.set_subgroup(1);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string res(Extractor(extractor).extract(callbacks, headers, empty_body));

  EXPECT_EQ("123", res);
}

TEST(Extraction, ExtractorWorkWithNewlines) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/users/123"}};
  envoy::api::v2::filter::http::Extraction extractor;
  extractor.mutable_body();
  extractor.set_regex("[\\S\\s]*");
  extractor.set_subgroup(0);
  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  std::string body("1\n2\n3");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };

  std::string res(Extractor(extractor).extract(callbacks, headers, bodyfunc));

  EXPECT_EQ(body, res);
}

TEST(Extraction, ExtractorFail) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/users/123"}};
  envoy::api::v2::filter::http::Extraction extractor;
  extractor.set_header(":path");
  extractor.set_regex("ILLEGAL REGEX \\ \\ \\ \\ a\\ \\a\\ a\\  \\d+)");
  extractor.set_subgroup(1);
  EXPECT_THAT_THROWS_MESSAGE(Extractor a(extractor), EnvoyException,
                             HasSubstr("Invalid regex"));
}

TEST(Extraction, ExtractorFailOnOutOfRangeGroup) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/users/123"}};
  envoy::api::v2::filter::http::Extraction extractor;
  extractor.set_header(":path");
  extractor.set_regex("(\\d+)");
  extractor.set_subgroup(123);
  EXPECT_THROW_WITH_MESSAGE(
      Extractor a(extractor), EnvoyException,
      "group 123 requested for regex with only 1 sub groups");
}

TEST(Transformer, transform) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
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
  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  transformer.transform(headers, &headers, body, callbacks);

  std::string res = body.toString();

  EXPECT_EQ("123456789", res);
  EXPECT_EQ("ABC", headers.get_("x-header"));
}

TEST(Transformer, transformSimple) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
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
  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  transformer.transform(headers, &headers, body, callbacks);

  std::string res = body.toString();

  EXPECT_EQ("123456789", res);
  EXPECT_EQ("ABC", headers.get_("x-header"));
}

TEST(Transformer, transformMultipleHeaderValues) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {"x-custom-header", "original value"},
                                         {":path", "/users/123"}};
  Buffer::OwnedImpl body;
  TransformationTemplate transformation;

  const auto &header = transformation.add_headers_to_append();
  header->set_key("x-custom-header");
  header->mutable_value()->set_text("{{upper(\"first value\")}}");
  const auto &header1 = transformation.add_headers_to_append();
  header1->set_key("x-custom-header");
  header1->mutable_value()->set_text("{{upper(\"second value\")}}");
  transformation.set_advanced_templates(false);

  InjaTransformer transformer(transformation);
  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  transformer.transform(headers, &headers, body, callbacks);


  auto lowerkey = Http::LowerCaseString("x-custom-header");
  auto result = headers.get(lowerkey);
  // Check original header value is preserved
  EXPECT_EQ("original value", result[0]->value().getStringView());
  // Check multiple transformed values are included
  EXPECT_EQ("FIRST VALUE", result[1]->value().getStringView());
  EXPECT_EQ("SECOND VALUE", result[2]->value().getStringView());
}

TEST(Transformer, transformHeaderAndHeadersToAppend) {
  Http::TestRequestHeaderMapImpl headers{{"x-custom-header", "original value"}};
  Buffer::OwnedImpl body;
  TransformationTemplate transformation;
  // define "headers"
  // this should overwrite existing headers with the same name
  (*transformation.mutable_headers())["x-custom-header"].set_text(
      "{{upper(\"overwritten value\")}}");
  // define "headers_to_append" 
  // these header values should be appended to the current x-custom-header
  const auto &header = transformation.add_headers_to_append();
  header->set_key("x-custom-header");
  header->mutable_value()->set_text("{{upper(\"first value\")}}");
  const auto &header1 = transformation.add_headers_to_append();
  header1->set_key("x-custom-header");
  header1->mutable_value()->set_text("{{upper(\"second value\")}}");
  transformation.set_advanced_templates(false);

  InjaTransformer transformer(transformation);
  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  transformer.transform(headers, &headers, body, callbacks);


  auto lowerkey = Http::LowerCaseString("x-custom-header");
  auto result = headers.get(lowerkey);
  // Check original header value is preserved
  EXPECT_EQ("OVERWRITTEN VALUE", result[0]->value().getStringView());
  // Check multiple transformed values are included
  EXPECT_EQ("FIRST VALUE", result[1]->value().getStringView());
  EXPECT_EQ("SECOND VALUE", result[2]->value().getStringView());
}

TEST(Transformer, transformSimpleNestedStructs) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
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
  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  transformer.transform(headers, &headers, body, callbacks);

  std::string res = body.toString();

  EXPECT_EQ("123456789", res);
  EXPECT_EQ("ABC", headers.get_("x-header"));
}

TEST(Transformer, transformPassthrough) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
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
  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  transformer.transform(headers, &headers, body, callbacks);

  std::string res = body.toString();

  EXPECT_EQ(emptyBody, res);
  EXPECT_EQ("default", headers.get_("x-header"));
}

TEST(Transformer, transformMergeExtractorsToBody) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
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
  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  transformer.transform(headers, &headers, body, callbacks);

  std::string res = body.toString();

  EXPECT_EQ("{\"ext1\":\"123\"}", res);
}

TEST(Transformer, transformBodyNotSet) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
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
  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  transformer.transform(headers, &headers, body, callbacks);

  std::string res = body.toString();

  EXPECT_EQ(originalBody, res);
  EXPECT_EQ("456", headers.get_("x-header"));
}

TEST(InjaTransformer, transformWithHyphens) {
  Http::TestRequestHeaderMapImpl headers{
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
  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  transformer.transform(headers, &headers, body, callbacks);

  std::string res = body.toString();

  EXPECT_THAT(res, HasSubstr("\"764b.0f_0f-7319-4b29-bbd0-887a39705a70\""));
}

TEST(InjaTransformer, RemoveHeadersUsingEmptyTemplate) {
  const std::string content_type = "content-type";
  Http::TestRequestHeaderMapImpl headers{
      {":method", "GET"}, {":path", "/foo"}, {content_type, "x-test"}};
  Buffer::OwnedImpl body("{}");

  TransformationTemplate transformation;
  envoy::api::v2::filter::http::InjaTemplate empty;

  (*transformation.mutable_headers())[content_type] = empty;

  InjaTransformer transformer(transformation);

  EXPECT_TRUE(headers.has(content_type));
  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_FALSE(headers.has(content_type));
}

TEST(InjaTransformer, DontParseBodyAndExtractFromIt) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
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

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), "json");
}

TEST(InjaTransformer, UseBodyFunction) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;
  transformation.set_parse_body_behavior(TransformationTemplate::DontParse);
  transformation.set_advanced_templates(true);

  transformation.mutable_body()->set_text("{{body()}} {{body()}}");

  InjaTransformer transformer(transformation);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  Buffer::OwnedImpl body("1");
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), "1 1");
}

TEST(InjaTransformer, UseDefaultNS) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;
  transformation.set_parse_body_behavior(TransformationTemplate::DontParse);
  transformation.set_advanced_templates(true);

  auto dynamic_meta = transformation.add_dynamic_metadata_values();
  dynamic_meta->set_key("foo");
  dynamic_meta->mutable_value()->set_text("{{body()}}");

  InjaTransformer transformer(transformation);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  EXPECT_CALL(callbacks.stream_info_,
              setDynamicMetadata(SoloHttpFilterNames::get().Transformation, _))
      .Times(1)
      .WillOnce(
          Invoke([](const std::string &, const ProtobufWkt::Struct &value) {
            auto field = value.fields().at("foo");
            EXPECT_EQ(field.string_value(), "1");
          }));
  Buffer::OwnedImpl body("1");
  transformer.transform(headers, &headers, body, callbacks);
}

TEST(InjaTransformer, UseCustomNS) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;
  transformation.set_parse_body_behavior(TransformationTemplate::DontParse);
  transformation.set_advanced_templates(true);

  auto dynamic_meta = transformation.add_dynamic_metadata_values();
  dynamic_meta->set_key("foo");
  dynamic_meta->set_metadata_namespace("foo.ns");
  dynamic_meta->mutable_value()->set_text("123");

  InjaTransformer transformer(transformation);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  EXPECT_CALL(callbacks.stream_info_, setDynamicMetadata("foo.ns", _)).Times(1);
  Buffer::OwnedImpl body;
  transformer.transform(headers, &headers, body, callbacks);
}

TEST(InjaTransformer, UseDynamicMetaTwice) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;

  auto dynamic_meta = transformation.add_dynamic_metadata_values();
  dynamic_meta->set_key("foo");
  dynamic_meta->mutable_value()->set_text("{{body()}}");
  dynamic_meta = transformation.add_dynamic_metadata_values();
  dynamic_meta->set_key("bar");
  dynamic_meta->mutable_value()->set_text("123");

  InjaTransformer transformer(transformation);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  EXPECT_CALL(callbacks.stream_info_,
              setDynamicMetadata(SoloHttpFilterNames::get().Transformation, _))
      .Times(2);
  Buffer::OwnedImpl body("1");
  transformer.transform(headers, &headers, body, callbacks);
}

TEST(InjaTransformer, UseEnvVar) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;
  transformation.mutable_body()->set_text("{{env(\"FOO\")}}");
  // set env before calling transformer
  TestEnvironment::setEnvVar("FOO", "BAR", 1);
  TestEnvironment::setEnvVar("EMPTY", "", 1);

  InjaTransformer transformer(transformation);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  Buffer::OwnedImpl body("1");
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), "BAR");
}

TEST(InjaTransformer, Base64EncodeTestString) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;

  auto test_string = "test";
  auto formatted_string = fmt::format("{{{{base64Encode(\"{}\")}}}}", test_string);

  transformation.mutable_body()->set_text(formatted_string);

  InjaTransformer transformer(transformation);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  Buffer::OwnedImpl body("");
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(Base64::decode(body.toString()), test_string);
}

TEST(InjaTransformer, Base64DecodeTestString) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;

  std::string test_string = "test";
  auto encoded_string = Base64::encode(test_string.c_str(), test_string.length());

  auto formatted_string = fmt::format("{{{{base64Decode(\"{}\")}}}}", encoded_string);

  transformation.mutable_body()->set_text(formatted_string);

  InjaTransformer transformer(transformation);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  Buffer::OwnedImpl body("");
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), test_string);
}

TEST(InjaTransformer, Base64Composed) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;

  transformation.mutable_body()->set_text("{{base64Decode(base64Encode(body()))}}");

  InjaTransformer transformer(transformation);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  auto test_string = "1";
  Buffer::OwnedImpl body(test_string);
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), test_string);
}

TEST(InjaTransformer, ParseBodyListUsingContext) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;
  transformation.mutable_body()->set_text(
      "{% for i in context() %}{{ i }}{% endfor %}");
  InjaTransformer transformer(transformation);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  Buffer::OwnedImpl body("[3,2,1]");
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), "321");
}

TEST(InjaTransformer, ParseFromClusterMetadata) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;
  transformation.mutable_body()->set_text("{{clusterMetadata(\"key\")}}");

  InjaTransformer transformer(transformation);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  envoy::config::core::v3::Metadata meta;
  meta.mutable_filter_metadata()->insert(
      {SoloHttpFilterNames::get().Transformation,
       MessageUtil::keyValueStruct("key", "val")});
  ON_CALL(*callbacks.cluster_info_, metadata())
      .WillByDefault(testing::ReturnRefOfCopy(meta));

  Buffer::OwnedImpl body("1");
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), "val");
}

TEST(InjaTransformer, ParseFromNilClusterInfo) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;
  transformation.mutable_body()->set_text("{{clusterMetadata(\"key\")}}");

  InjaTransformer transformer(transformation);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  callbacks.cluster_info_.reset();
  callbacks.cluster_info_ = nullptr;

  Buffer::OwnedImpl body("1");
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), "");
}

TEST(Transformer, transformHeaderAndHeadersToRemove) {
  Http::TestRequestHeaderMapImpl headers{
    {"x-custom-header1", "custom-value1"},
    {"x-custom-header2-repeated", "custom-value2"},
    {"x-custom-header3", "custom-value3"},
    {"x-custom-header2-repeated", "custom-value4"},
  };
  Buffer::OwnedImpl body;
  TransformationTemplate transformation;

  // check that all headers are present
  auto lowerkey1 = Http::LowerCaseString("x-custom-header1");
  auto lowerkey2 = Http::LowerCaseString("x-custom-header2-repeated");
  auto lowerkey3 = Http::LowerCaseString("x-custom-header3");
  auto result1 = headers.get(lowerkey1);
  auto result2 = headers.get(lowerkey2);
  auto result3 = headers.get(lowerkey3);
  EXPECT_EQ(1, result1.size());
  EXPECT_EQ(2, result2.size());
  EXPECT_EQ(1, result3.size());
  EXPECT_EQ("custom-value1", result1[0]->value().getStringView());
  EXPECT_EQ("custom-value2", result2[0]->value().getStringView());
  EXPECT_EQ("custom-value4", result2[1]->value().getStringView());
  EXPECT_EQ("custom-value3", result3[0]->value().getStringView());

  // perform the removal of header2 only
  transformation.add_headers_to_remove("x-custom-header2-repeated");
  transformation.add_headers_to_remove("x-custom-header1");
  InjaTransformer transformer(transformation);
  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  transformer.transform(headers, &headers, body, callbacks);

  // ensure that x-custom-header2-repeated is removed
  auto result1_postremoval = headers.get(lowerkey1);
  auto result2_postremoval = headers.get(lowerkey2);
  auto result3_postremoval = headers.get(lowerkey3);
  EXPECT_EQ(0, result1_postremoval.size());
  EXPECT_EQ(0, result2_postremoval.size());
  EXPECT_EQ(1, result3_postremoval.size());
  EXPECT_EQ("custom-value3", result3[0]->value().getStringView());
}

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
