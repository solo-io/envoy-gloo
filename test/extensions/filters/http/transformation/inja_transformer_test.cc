#include "source/extensions/filters/http/solo_well_known_names.h"
#include "source/extensions/filters/http/transformation/inja_transformer.h"
#include "source/common/common/base64.h"
#include "source/common/common/random_generator.h"

#include "test/mocks/common.h"
#include "test/mocks/http/mocks.h"
#include "test/mocks/server/mocks.h"
#include "test/mocks/thread_local/mocks.h"
#include "test/mocks/tracing/mocks.h"
#include "test/mocks/upstream/mocks.h"
#include "test/test_common/environment.h"

#include "fmt/format.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <memory>

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

class TransformerInstanceTest : public testing::Test {
protected:
  NiceMock<Server::Configuration::MockServerFactoryContext> factory_context_;
  NiceMock<Random::MockRandomGenerator> rng_;
  NiceMock<ThreadLocal::MockInstance> tls_;
};

void fill_slot(
      ThreadLocal::SlotPtr& slot,
      const Http::RequestOrResponseHeaderMap &header_map,
      const Http::RequestHeaderMap *request_headers,
      GetBodyFunc &body,
      const std::unordered_map<std::string, absl::string_view> &extractions,
      const std::unordered_map<std::string, std::string> &destructive_extractions,
      const nlohmann::json &context,
      const std::unordered_map<std::string, std::string> &environ,
      const envoy::config::core::v3::Metadata *cluster_metadata) {
  slot->set([](Event::Dispatcher&) -> ThreadLocal::ThreadLocalObjectSharedPtr {
          return std::make_shared<ThreadLocalTransformerContext>();
  });
  auto& typed_slot = slot->getTyped<ThreadLocalTransformerContext>();
  typed_slot.header_map_ = &header_map;
  typed_slot.request_headers_ = request_headers;
  typed_slot.body_ = &body;
  typed_slot.extractions_ = &extractions;
  typed_slot.destructive_extractions_ = &destructive_extractions;
  typed_slot.context_ = &context;
  typed_slot.environ_ = &environ;
  typed_slot.cluster_metadata_ = cluster_metadata;
}

TEST_F(TransformerInstanceTest, ReplacesValueFromContext) {
  json originalbody;
  originalbody["field1"] = "value1";
  Http::TestRequestHeaderMapImpl headers;
  std::unordered_map<std::string, absl::string_view> extractions;
  std::unordered_map<std::string, std::string> destructive_extractions;
  std::unordered_map<std::string, std::string> env;
  envoy::config::core::v3::Metadata *cluster_metadata{};

  auto slot = tls_.allocateSlot();
  fill_slot(slot,
          headers, &headers, empty_body, extractions, destructive_extractions, originalbody, env, cluster_metadata);
  TransformerInstance t(*slot, rng_);

  auto res = t.render(t.parse("{{field1}}"));

  EXPECT_EQ(originalbody["field1"], res);
}

TEST_F(TransformerInstanceTest, ReplacesValueFromInlineHeader) {
  json originalbody;
  originalbody["field1"] = "value1";
  std::string path = "/getsomething";

  Http::TestRequestHeaderMapImpl headers{
      {":method", "GET"}, {":authority", "www.solo.io"}, {":path", path}};
  std::unordered_map<std::string, absl::string_view> extractions;
  std::unordered_map<std::string, std::string> destructive_extractions;
  std::unordered_map<std::string, std::string> env;
  envoy::config::core::v3::Metadata *cluster_metadata{};

  auto slot = tls_.allocateSlot();
  fill_slot(slot,
          headers, &headers, empty_body, extractions, destructive_extractions, originalbody, env, cluster_metadata);

  TransformerInstance t(*slot, rng_);

  auto res = t.render(t.parse("{{header(\":path\")}}"));

  EXPECT_EQ(path, res);
}

TEST_F(TransformerInstanceTest, ReplacesValueFromCustomHeader) {
  json originalbody;
  originalbody["field1"] = "value1";
  std::string header = "blah blah";
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"},
                                         {"x-custom-header", header}};
  std::unordered_map<std::string, absl::string_view> extractions;
  std::unordered_map<std::string, std::string> destructive_extractions;
  std::unordered_map<std::string, std::string> env;
  envoy::config::core::v3::Metadata *cluster_metadata{};

  auto slot = tls_.allocateSlot();
  fill_slot(slot,
          headers, &headers, empty_body, extractions, destructive_extractions, originalbody, env, cluster_metadata);

  TransformerInstance t(*slot, rng_);

  auto res = t.render(t.parse("{{header(\"x-custom-header\")}}"));

  EXPECT_EQ(header, res);
}

TEST_F(TransformerInstanceTest, ReplaceFromExtracted) {
  json originalbody;
  std::unordered_map<std::string, absl::string_view> extractions;
  std::unordered_map<std::string, std::string> destructive_extractions;
  absl::string_view field = "res";
  extractions["f"] = field;
  Http::TestRequestHeaderMapImpl headers;
  std::unordered_map<std::string, std::string> env;
  envoy::config::core::v3::Metadata *cluster_metadata{};

  auto slot = tls_.allocateSlot();
  fill_slot(slot,
          headers, &headers, empty_body, extractions, destructive_extractions, originalbody, env, cluster_metadata);

  TransformerInstance t(*slot, rng_);

  auto res = t.render(t.parse("{{extraction(\"f\")}}"));

  EXPECT_EQ(field, res);
}

TEST_F(TransformerInstanceTest, ReplaceFromNonExistentExtraction) {
  json originalbody;
  std::unordered_map<std::string, absl::string_view> extractions;
  std::unordered_map<std::string, std::string> destructive_extractions;
  extractions["foo"] = absl::string_view("bar");
  Http::TestRequestHeaderMapImpl headers;
  std::unordered_map<std::string, std::string> env;
  envoy::config::core::v3::Metadata *cluster_metadata{};

  auto slot = tls_.allocateSlot();
  fill_slot(slot,
          headers, &headers, empty_body, extractions, destructive_extractions, originalbody, env, cluster_metadata);

  TransformerInstance t(*slot, rng_);

  auto res = t.render(t.parse("{{extraction(\"notsuchfield\")}}"));

  EXPECT_EQ("", res);
}

TEST_F(TransformerInstanceTest, Environment) {
  json originalbody;
  std::unordered_map<std::string, absl::string_view> extractions;
  std::unordered_map<std::string, std::string> destructive_extractions;
  Http::TestRequestHeaderMapImpl headers;
  std::unordered_map<std::string, std::string> env;
  envoy::config::core::v3::Metadata *cluster_metadata{};
  env["FOO"] = "BAR";

  auto slot = tls_.allocateSlot();
  fill_slot(slot,
          headers, &headers, empty_body, extractions, destructive_extractions, originalbody, env, cluster_metadata);

  TransformerInstance t(*slot, rng_);

  auto res = t.render(t.parse("{{env(\"FOO\")}}"));
  EXPECT_EQ("BAR", res);
}

TEST_F(TransformerInstanceTest, EmptyEnvironment) {
  json originalbody;
  std::unordered_map<std::string, absl::string_view> extractions;
  std::unordered_map<std::string, std::string> destructive_extractions;
  Http::TestRequestHeaderMapImpl headers;

  std::unordered_map<std::string, std::string> env;
  envoy::config::core::v3::Metadata *cluster_metadata{};

  auto slot = tls_.allocateSlot();
  fill_slot(slot,
          headers, &headers, empty_body, extractions, destructive_extractions, originalbody, env, cluster_metadata);

  TransformerInstance t(*slot, rng_);

  auto res = t.render(t.parse("{{env(\"FOO\")}}"));
  EXPECT_EQ("", res);
}

TEST_F(TransformerInstanceTest, ClusterMetadata) {
  json originalbody;
  std::unordered_map<std::string, absl::string_view> extractions;
  std::unordered_map<std::string, std::string> destructive_extractions;
  Http::TestRequestHeaderMapImpl headers;

  std::unordered_map<std::string, std::string> env;

  envoy::config::core::v3::Metadata cluster_metadata;
  cluster_metadata.mutable_filter_metadata()->insert(
      {SoloHttpFilterNames::get().Transformation,
       MessageUtil::keyValueStruct("io.solo.hostname", "foo.example.com")});

  auto slot = tls_.allocateSlot();
  fill_slot(slot,
          headers, &headers, empty_body, extractions, destructive_extractions, originalbody, env, &cluster_metadata);

  TransformerInstance t(*slot, rng_);

  auto res = t.render(t.parse("{{clusterMetadata(\"io.solo.hostname\")}}"));
  EXPECT_EQ("foo.example.com", res);
}

TEST_F(TransformerInstanceTest, EmptyClusterMetadata) {
  json originalbody;
  std::unordered_map<std::string, absl::string_view> extractions;
  std::unordered_map<std::string, std::string> destructive_extractions;
  Http::TestRequestHeaderMapImpl headers;

  std::unordered_map<std::string, std::string> env;
  envoy::config::core::v3::Metadata *cluster_metadata{};

  auto slot = tls_.allocateSlot();
  fill_slot(slot,
          headers, &headers, empty_body, extractions, destructive_extractions, originalbody, env, cluster_metadata);

  TransformerInstance t(*slot, rng_);

  auto res = t.render(t.parse("{{clusterMetadata(\"io.solo.hostname\")}}"));
  EXPECT_EQ("", res);
}

TEST_F(TransformerInstanceTest, RequestHeaders) {
  json originalbody;
  std::unordered_map<std::string, absl::string_view> extractions;
  std::unordered_map<std::string, std::string> destructive_extractions;
  Http::TestResponseHeaderMapImpl response_headers{{":status", "200"}};
  Http::TestRequestHeaderMapImpl request_headers{{":method", "GET"}};

  std::unordered_map<std::string, std::string> env;
  envoy::config::core::v3::Metadata *cluster_metadata{};

  auto slot = tls_.allocateSlot();
  fill_slot(slot,
          response_headers, &request_headers, empty_body, extractions, destructive_extractions, originalbody, env, cluster_metadata);

  TransformerInstance t(*slot, rng_);

  auto res = t.render(
      t.parse("{{header(\":status\")}}-{{request_header(\":method\")}}"));
  EXPECT_EQ("200-GET", res);
}

TEST(Extraction, ExtractIdFromHeader) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/users/123"}};
  ExtractionApi extractor;
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
  ExtractionApi extractor;
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
  ExtractionApi extractor;
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
  ExtractionApi extractor;
  extractor.set_header(":path");
  extractor.set_regex("(\\d+)");
  extractor.set_subgroup(123);
  EXPECT_THROW_WITH_MESSAGE(
      Extractor a(extractor), EnvoyException,
      "group 123 requested for regex with only 1 sub groups");
}

class TransformerTest : public TransformerInstanceTest {};

TEST_F(TransformerTest, transform) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {"x-test", "789"},
                                         {":path", "/users/123"}};
  Buffer::OwnedImpl body("{\"a\":\"456\"}");

  ExtractionApi extractor;
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

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);
  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  transformer.transform(headers, &headers, body, callbacks);

  std::string res = body.toString();

  EXPECT_EQ("123456789", res);
  EXPECT_EQ("ABC", headers.get_("x-header"));
}

TEST_F(TransformerTest, transformSimple) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {"x-test", "789"},
                                         {":path", "/users/123"}};
  Buffer::OwnedImpl body("{\"a\":\"456\"}");

  ExtractionApi extractor;
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

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);
  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  transformer.transform(headers, &headers, body, callbacks);

  std::string res = body.toString();

  EXPECT_EQ("123456789", res);
  EXPECT_EQ("ABC", headers.get_("x-header"));
}

TEST_F(TransformerTest, transformMultipleHeaderValues) {
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

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);
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

TEST_F(TransformerTest, transformHeaderAndHeadersToAppend) {
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

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);
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

TEST_F(TransformerTest, transformSimpleNestedStructs) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {"x-test", "789"},
                                         {":path", "/users/123"}};
  Buffer::OwnedImpl body("{\"a\":\"456\"}");

  ExtractionApi extractor;
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

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);
  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  transformer.transform(headers, &headers, body, callbacks);

  std::string res = body.toString();

  EXPECT_EQ("123456789", res);
  EXPECT_EQ("ABC", headers.get_("x-header"));
}

TEST_F(TransformerTest, transformPassthrough) {
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

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);
  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  transformer.transform(headers, &headers, body, callbacks);

  std::string res = body.toString();

  EXPECT_EQ(emptyBody, res);
  EXPECT_EQ("default", headers.get_("x-header"));
}

TEST_F(TransformerTest, transformMergeExtractorsToBody) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {"x-test", "789"},
                                         {":path", "/users/123"}};
  // in passthrough mode the filter gives us an empty body
  std::string emptyBody = "";
  Buffer::OwnedImpl body(emptyBody);

  TransformationTemplate transformation;

  transformation.mutable_merge_extractors_to_body();

  ExtractionApi extractor;
  extractor.set_header(":path");
  extractor.set_regex("/users/(\\d+)");
  extractor.set_subgroup(1);
  (*transformation.mutable_extractors())["ext1"] = extractor;

  transformation.set_advanced_templates(false);

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);
  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  transformer.transform(headers, &headers, body, callbacks);

  std::string res = body.toString();

  EXPECT_EQ("{\"ext1\":\"123\"}", res);
}

TEST_F(TransformerTest, transformMergeReplaceExtractorsToBody) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {"x-test", "789"},
                                         {":path", "/users/123"}};
  // in passthrough mode the filter gives us an empty body
  std::string emptyBody = "";
  Buffer::OwnedImpl body(emptyBody);

  TransformationTemplate transformation;

  transformation.mutable_merge_extractors_to_body();

  ExtractionApi extractor;
  extractor.set_header(":path");
  extractor.set_regex("/users/(\\d+)");
  extractor.set_subgroup(1);
  extractor.mutable_replacement_text()->set_value("456");
  extractor.set_mode(ExtractionApi::SINGLE_REPLACE);
  (*transformation.mutable_extractors())["ext1"] = extractor;

  transformation.set_advanced_templates(false);

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);
  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  transformer.transform(headers, &headers, body, callbacks);

  std::string res = body.toString();

  // With replacement text, we replace the portion of the header value
  // in the specified subgroup with the replacement text, and then the
  // value of the replaced input is the value of the extraction. 
  EXPECT_EQ("{\"ext1\":\"/users/456\"}", res);
}

TEST_F(TransformerTest, transformBodyNotSet) {
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

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);
  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  transformer.transform(headers, &headers, body, callbacks);

  std::string res = body.toString();

  EXPECT_EQ(originalBody, res);
  EXPECT_EQ("456", headers.get_("x-header"));
}

class InjaTransformerTest : public TransformerInstanceTest {};

TEST_F(InjaTransformerTest, transformWithHyphens) {
  Http::TestRequestHeaderMapImpl headers{
      {":method", "GET"},
      {":path", "/accounts/764b.0f_0f-7319-4b29-bbd0-887a39705a70"}};
  Buffer::OwnedImpl body("{}");

  ExtractionApi extractor;
  extractor.set_header(":path");
  extractor.set_regex("/accounts/([\\-._[:alnum:]]+)");
  extractor.set_subgroup(1);

  TransformationTemplate transformation;

  (*transformation.mutable_extractors())["id"] = extractor;

  transformation.set_advanced_templates(false);
  transformation.mutable_merge_extractors_to_body();

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);
  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  transformer.transform(headers, &headers, body, callbacks);

  std::string res = body.toString();

  EXPECT_THAT(res, HasSubstr("\"764b.0f_0f-7319-4b29-bbd0-887a39705a70\""));
}

TEST_F(InjaTransformerTest, RemoveHeadersUsingEmptyTemplate) {
  const std::string content_type = "content-type";
  Http::TestRequestHeaderMapImpl headers{
      {":method", "GET"}, {":path", "/foo"}, {content_type, "x-test"}};
  Buffer::OwnedImpl body("{}");

  TransformationTemplate transformation;
  envoy::api::v2::filter::http::InjaTemplate empty;

  (*transformation.mutable_headers())[content_type] = empty;

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  EXPECT_TRUE(headers.has(content_type));
  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_FALSE(headers.has(content_type));
}

TEST_F(InjaTransformerTest, DontParseBodyAndExtractFromIt) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  Buffer::OwnedImpl body("not json body");

  TransformationTemplate transformation;
  transformation.set_parse_body_behavior(TransformationTemplate::DontParse);
  transformation.set_advanced_templates(true);

  ExtractionApi extractor;
  extractor.mutable_body();
  extractor.set_regex("not ([\\-._[:alnum:]]+) body");
  extractor.set_subgroup(1);
  extractor.set_mode(ExtractionApi::EXTRACT);
  (*transformation.mutable_extractors())["param"] = extractor;

  transformation.mutable_body()->set_text("{{extraction(\"param\")}}");

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), "json");
}

TEST_F(InjaTransformerTest, DontParseBodyAndExtractFromReplacementText) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  Buffer::OwnedImpl body("not json body");

  TransformationTemplate transformation;
  transformation.set_parse_body_behavior(TransformationTemplate::DontParse);
  transformation.set_advanced_templates(true);

  ExtractionApi extractor;
  extractor.mutable_body();
  extractor.set_regex("not ([\\-._[:alnum:]]+) body");
  extractor.set_subgroup(1);
  extractor.mutable_replacement_text()->set_value("JSON");
  extractor.set_mode(ExtractionApi::SINGLE_REPLACE);
  (*transformation.mutable_extractors())["param"] = extractor;

  transformation.mutable_body()->set_text("{{extraction(\"param\")}}");

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), "not JSON body");
}

TEST_F(InjaTransformerTest, DestructiveAndNonDestructiveExtractors) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  Buffer::OwnedImpl body("not json body");

  TransformationTemplate transformation;
  transformation.set_parse_body_behavior(TransformationTemplate::DontParse);
  transformation.set_advanced_templates(true);

  ExtractionApi extractor;
  extractor.mutable_body();
  extractor.set_regex("not ([\\-._[:alnum:]]+) body");
  extractor.set_subgroup(1);
  extractor.set_mode(ExtractionApi::EXTRACT);
  (*transformation.mutable_extractors())["param"] = extractor;

  ExtractionApi destructive_extractor;
  destructive_extractor.mutable_body();
  destructive_extractor.set_regex("not ([\\-._[:alnum:]]+) body");
  destructive_extractor.set_subgroup(1);
  destructive_extractor.mutable_replacement_text()->set_value("JSON");
  destructive_extractor.set_mode(ExtractionApi::SINGLE_REPLACE);
  (*transformation.mutable_extractors())["destructive_param"] = destructive_extractor;

  transformation.mutable_body()->set_text("{{extraction(\"param\")}} {{extraction(\"destructive_param\")}}");

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), "json not JSON body");
}

TEST_F(InjaTransformerTest, UseBodyFunction) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;
  transformation.set_parse_body_behavior(TransformationTemplate::DontParse);
  transformation.set_advanced_templates(true);

  transformation.mutable_body()->set_text("{{body()}} {{body()}}");

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  Buffer::OwnedImpl body("1");
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), "1 1");
}

TEST_F(InjaTransformerTest, MergeJsonKeys) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;


  // TransformationTemplate transformation;
  // transformation.mutable_body()->set_text("{\"ext2\": \"{{header(\":path\")}\" }");

  envoy::api::v2::filter::http::InjaTemplate inja_template;
  inja_template.set_text("\"{{header(\":path\")}}\"");
  envoy::api::v2::filter::http::MergeJsonKeys_OverridableTemplate tmpl;
  (*tmpl.mutable_tmpl()) = inja_template;
  (*transformation.mutable_merge_json_keys()->mutable_json_keys())["ext2"] = tmpl;
  (*transformation.mutable_merge_json_keys()->mutable_json_keys())["ext1"] = tmpl;

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  Buffer::OwnedImpl body("{\"ext1\":\"123\"}");
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), "{\"ext1\":\"/foo\",\"ext2\":\"/foo\"}");
}

TEST_F(InjaTransformerTest, MergeJsonKeysNoOverrideEmpty ) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;

  envoy::api::v2::filter::http::InjaTemplate inja_template;
  // Should return "" and therefore not write the key at all
  inja_template.set_text("{{header(\":status\")}}");
  envoy::api::v2::filter::http::MergeJsonKeys_OverridableTemplate tmpl;
  (*tmpl.mutable_tmpl()) = inja_template;
  (*transformation.mutable_merge_json_keys()->mutable_json_keys())["ext2"] = tmpl;

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  Buffer::OwnedImpl body("{\"ext1\":\"123\"}");
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), "{\"ext1\":\"123\"}");
}

TEST_F(InjaTransformerTest, MergeJsonKeysEmptyBody ) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;

  envoy::api::v2::filter::http::InjaTemplate inja_template;
  // Should return "" and therefore not write the key at all
  inja_template.set_text("\"{{header(\":method\")}}\"");
  envoy::api::v2::filter::http::MergeJsonKeys_OverridableTemplate tmpl;
  (*tmpl.mutable_tmpl()) = inja_template;
  (*transformation.mutable_merge_json_keys()->mutable_json_keys())["ext2"] = tmpl;

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  Buffer::OwnedImpl body("");
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), "{\"ext2\":\"GET\"}");
}

TEST_F(InjaTransformerTest, MergeJsonKeysEmptyJSONBody ) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;

  envoy::api::v2::filter::http::InjaTemplate inja_template;
  // Should return "" and therefore not write the key at all
  inja_template.set_text("\"{{header(\":method\")}}\"");
  envoy::api::v2::filter::http::MergeJsonKeys_OverridableTemplate tmpl;
  (*tmpl.mutable_tmpl()) = inja_template;
  (*transformation.mutable_merge_json_keys()->mutable_json_keys())["ext2"] = tmpl;

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  Buffer::OwnedImpl body("{}");
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), "{\"ext2\":\"GET\"}");
}

TEST_F(InjaTransformerTest, UseDefaultNS) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;
  transformation.set_parse_body_behavior(TransformationTemplate::DontParse);
  transformation.set_advanced_templates(true);

  auto dynamic_meta = transformation.add_dynamic_metadata_values();
  dynamic_meta->set_key("foo");
  dynamic_meta->mutable_value()->set_text("{{body()}}");

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

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

TEST_F(InjaTransformerTest, UseDefaultNSStructureData) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;
  transformation.set_parse_body_behavior(TransformationTemplate::DontParse);
  transformation.set_advanced_templates(true);

  auto dynamic_meta = transformation.add_dynamic_metadata_values();
  dynamic_meta->set_key("foo");
  dynamic_meta->mutable_value()->set_text("{{body()}}");
  dynamic_meta->set_json_to_proto(true);

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  EXPECT_CALL(callbacks.stream_info_,
              setDynamicMetadata(SoloHttpFilterNames::get().Transformation, _))
      .Times(1)
      .WillOnce(
          Invoke([](const std::string &, const ProtobufWkt::Struct &value) {
            auto field = value.fields().at("foo");
            EXPECT_EQ(field.has_list_value(), true);
          }));
  Buffer::OwnedImpl body("[1, 2]");
  transformer.transform(headers, &headers, body, callbacks);
}

TEST_F(InjaTransformerTest, UseCustomNS) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;
  transformation.set_parse_body_behavior(TransformationTemplate::DontParse);
  transformation.set_advanced_templates(true);

  auto dynamic_meta = transformation.add_dynamic_metadata_values();
  dynamic_meta->set_key("foo");
  dynamic_meta->set_metadata_namespace("foo.ns");
  dynamic_meta->mutable_value()->set_text("{{body()}}");
  dynamic_meta->set_json_to_proto(true);

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  EXPECT_CALL(callbacks.stream_info_,
              setDynamicMetadata("foo.ns", _))
      .Times(1)
      .WillOnce(
          Invoke([](const std::string &, const ProtobufWkt::Struct &value) {
            auto field = value.fields().at("foo");
            EXPECT_EQ(field.number_value(), 123);
          }));
  Buffer::OwnedImpl body("123");
  transformer.transform(headers, &headers, body, callbacks);
}

TEST_F(InjaTransformerTest, UseDynamicMetaTwice) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;

  auto dynamic_meta = transformation.add_dynamic_metadata_values();
  dynamic_meta->set_key("foo");
  dynamic_meta->mutable_value()->set_text("{{body()}}");
  dynamic_meta = transformation.add_dynamic_metadata_values();
  dynamic_meta->set_key("bar");
  dynamic_meta->mutable_value()->set_text("123");

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  EXPECT_CALL(callbacks.stream_info_,
              setDynamicMetadata(SoloHttpFilterNames::get().Transformation, _))
      .Times(2);
  Buffer::OwnedImpl body("1");
  transformer.transform(headers, &headers, body, callbacks);
}

TEST_F(InjaTransformerTest, UseEnvVar) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;
  transformation.mutable_body()->set_text("{{env(\"FOO\")}}");
  // set env before calling transformer
  TestEnvironment::setEnvVar("FOO", "BAR", 1);
  TestEnvironment::setEnvVar("EMPTY", "", 1);

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  Buffer::OwnedImpl body("1");
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), "BAR");
}

TEST_F(InjaTransformerTest, Base64EncodeTestString) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;

  auto test_string = "test";
  auto formatted_string = fmt::format("{{{{base64_encode(\"{}\")}}}}", test_string);

  transformation.mutable_body()->set_text(formatted_string);

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  Buffer::OwnedImpl body("");
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(Base64::decode(body.toString()), test_string);
}

TEST_F(InjaTransformerTest, Base64DecodeTestString) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;

  std::string test_string = "test";
  auto encoded_string = Base64::encode(test_string.c_str(), test_string.length());

  auto formatted_string = fmt::format("{{{{base64_decode(\"{}\")}}}}", encoded_string);

  transformation.mutable_body()->set_text(formatted_string);

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  Buffer::OwnedImpl body("");
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), test_string);
}

TEST_F(InjaTransformerTest, Base64UrlDecodeJWT) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;

  // Test string that produced the base64url underscore
  auto expect_string = "aaoð";
  // JWT with an underscore which is an invalid base64 character
  auto encoded_string = "eyJzdWIiOiJhYW_DsCJ9";

  auto formatted_string = fmt::format("{{{{base64url_decode(\"{}\")}}}}", encoded_string);

  transformation.mutable_body()->set_text(formatted_string);

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  Buffer::OwnedImpl body("");
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_NE(std::string::npos, body.toString().find(expect_string));
}

TEST_F(InjaTransformerTest, Base64Composed) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;

  transformation.mutable_body()->set_text("{{base64_decode(base64_encode(body()))}}");

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  auto test_string = "1";
  Buffer::OwnedImpl body(test_string);
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), test_string);
}

TEST_F(InjaTransformerTest, DecodeInvalidBase64) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;

  transformation.mutable_body()->set_text("{{base64_decode(\"INVALID BASE64\")}}");

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  Buffer::OwnedImpl body("");
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), "");
}

TEST_F(InjaTransformerTest, Substring) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;

  transformation.mutable_body()->set_text("{{substring(body(), 1, 2)}}");

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  auto test_string = "123";
  Buffer::OwnedImpl body(test_string);
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), "23");
}

TEST_F(InjaTransformerTest, SubstringTwoArguments) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;

  transformation.mutable_body()->set_text("{{substring(body(), 1)}}");

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  auto test_string = "123";
  Buffer::OwnedImpl body(test_string);
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), "23");
}

TEST_F(InjaTransformerTest, WordCount) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;

  transformation.mutable_body()->set_text("{{word_count(body())}}");
  transformation.set_parse_body_behavior(TransformationTemplate::DontParse);

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  auto test_string = "why don't you accept me";
  Buffer::OwnedImpl body(test_string);
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), "5");
}

TEST_F(InjaTransformerTest, WordCountWeirdSpacing) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;

  transformation.mutable_body()->set_text("{{word_count(body())}}");
  transformation.set_parse_body_behavior(TransformationTemplate::DontParse);

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  auto test_string = "  why  don't   you \t\t accept me   ";
  Buffer::OwnedImpl body(test_string);
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), "5");
}

TEST_F(InjaTransformerTest, WordCountJSON) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;
  transformation.set_advanced_templates(true);

  auto dynamic_meta = transformation.add_dynamic_metadata_values();
  dynamic_meta->set_key("foo");
  dynamic_meta->mutable_value()->set_text("{{word_count(body())}}");

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  EXPECT_CALL(callbacks.stream_info_,
              setDynamicMetadata(SoloHttpFilterNames::get().Transformation, _))
      .Times(1)
      .WillOnce(
          Invoke([](const std::string &, const ProtobufWkt::Struct &value) {
            auto field = value.fields().at("foo");
            EXPECT_EQ(field.string_value(), "12");
          }));
  auto test_string = "{\"a\": \"Hal, what's the meaning of life?\", \"b\": \"I don't know John\"}";
  Buffer::OwnedImpl body(test_string);
  transformer.transform(headers, &headers, body, callbacks);
}

TEST_F(InjaTransformerTest, SubstringOutOfBounds) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  auto test_string = "123";

  // case: start index is greater than string length
  transformation.mutable_body()->set_text("{{substring(body(), 10, 1)}}");
  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);
  Buffer::OwnedImpl body(test_string);
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), "");

  // case: start index is negative
  transformation.mutable_body()->set_text("{{substring(body(), -1, 1)}}");
  InjaTransformer transformer2(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);
  body = Buffer::OwnedImpl(test_string);
  transformer2.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), "");

  // case: substring length is greater than string length
  transformation.mutable_body()->set_text("{{substring(body(), 0, 10)}}");
  InjaTransformer transformer3(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);
  body = Buffer::OwnedImpl(test_string);
  transformer3.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), "123");

  // case: substring length is negative
  transformation.mutable_body()->set_text("{{substring(body(), 0, -1)}}");
  InjaTransformer transformer4(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);
  body = Buffer::OwnedImpl(test_string);
  transformer4.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), "123");
}

TEST_F(InjaTransformerTest, SubstringNonIntegerArguments) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  auto test_string = "123";

  // case: start index is not an integer
  transformation.mutable_body()->set_text("{{substring(body(), \"a\", 1)}}");
  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);
  Buffer::OwnedImpl body(test_string);
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), "");

  // case: substring length is not an integer
  transformation.mutable_body()->set_text("{{substring(body(), 0, \"a\")}}");
  InjaTransformer transformer2(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);
  body = Buffer::OwnedImpl(test_string);
  transformer2.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), "");
}

TEST_F(InjaTransformerTest, ParseBodyListUsingContext) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;
  transformation.mutable_body()->set_text(
      "{% for i in context() %}{{ i }}{% endfor %}");
  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  Buffer::OwnedImpl body("[3,2,1]");
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), "321");
}

TEST_F(InjaTransformerTest, ParseFromClusterMetadata) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;
  transformation.mutable_body()->set_text("{{clusterMetadata(\"key\")}}");

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

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

TEST_F(InjaTransformerTest, SetSpanNameNullRoute) {
  std::string transformer_span_name = "TRANSFORMER_SPAN_NAME";
  TransformationTemplate transformation;
  transformation.mutable_span_transformer()->mutable_name()->set_text(transformer_span_name);

  Http::TestRequestHeaderMapImpl headers{};
  Buffer::OwnedImpl body("");
  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);
  std::unique_ptr<Tracing::MockSpan> mock_span = std::make_unique<Tracing::MockSpan>();
  const std::unique_ptr<Router::MockDecorator> mock_decorator = std::make_unique<NiceMock<Router::MockDecorator>>();
  ON_CALL(callbacks, route).WillByDefault(Return(nullptr));
  EXPECT_CALL(callbacks, activeSpan).WillOnce(ReturnRef(*mock_span));
  EXPECT_CALL(*mock_span, setOperation(transformer_span_name)).Times(1);

  transformer.transform(headers, &headers, body, callbacks);
}

TEST_F(InjaTransformerTest, SetSpanNameNullRouteDecorator) {
  std::string transformer_span_name = "TRANSFORMER_SPAN_NAME";
  TransformationTemplate transformation;
  transformation.mutable_span_transformer()->mutable_name()->set_text(transformer_span_name);

  Http::TestRequestHeaderMapImpl headers{};
  Buffer::OwnedImpl body("");
  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);
  std::unique_ptr<Tracing::MockSpan> mock_span = std::make_unique<Tracing::MockSpan>();
  const std::unique_ptr<Router::MockDecorator> mock_decorator = std::make_unique<NiceMock<Router::MockDecorator>>();
  ON_CALL(*callbacks.route_, decorator).WillByDefault(Return(nullptr));
  EXPECT_CALL(callbacks, activeSpan).WillOnce(ReturnRef(*mock_span));
  EXPECT_CALL(*mock_span, setOperation(transformer_span_name)).Times(1);

  transformer.transform(headers, &headers, body, callbacks);
}

TEST_F(InjaTransformerTest, SetSpanNameEmptyRouteDecorator) {
  std::string transformer_span_name = "TRANSFORMER_SPAN_NAME";
  TransformationTemplate transformation;
  transformation.mutable_span_transformer()->mutable_name()->set_text(transformer_span_name);

  Http::TestRequestHeaderMapImpl headers{};
  Buffer::OwnedImpl body("");
  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);
  std::unique_ptr<Tracing::MockSpan> mock_span = std::make_unique<Tracing::MockSpan>();
  const std::unique_ptr<Router::MockDecorator> mock_decorator = std::make_unique<NiceMock<Router::MockDecorator>>();
  EXPECT_CALL(*callbacks.route_, decorator).WillRepeatedly(Return(mock_decorator.get()));
  ON_CALL(*mock_decorator, getOperation()).WillByDefault(ReturnRef(""));
  EXPECT_CALL(callbacks, activeSpan).WillOnce(ReturnRef(*mock_span));
  EXPECT_CALL(*mock_span, setOperation(transformer_span_name)).Times(1);

  transformer.transform(headers, &headers, body, callbacks);
}

TEST_F(InjaTransformerTest, SetSpanNameNonEmptyRouteDecorator) {
  // Ensure that if route->decorator->operation is set, that it overrides the
  // transformer value
  std::string transformer_span_name = "TRANSFORMER_SPAN_NAME";
  std::string decorator_span_Name = "DECORATOR_SPAN_NAME";
  TransformationTemplate transformation;
  transformation.mutable_span_transformer()->mutable_name()->set_text(transformer_span_name);

  Http::TestRequestHeaderMapImpl headers{};
  Buffer::OwnedImpl body("");
  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);
  const std::unique_ptr<Router::MockDecorator> mock_decorator = std::make_unique<NiceMock<Router::MockDecorator>>();
  EXPECT_CALL(*callbacks.route_, decorator).WillRepeatedly(Return(mock_decorator.get()));
  ON_CALL(*mock_decorator, getOperation()).WillByDefault(ReturnRef(decorator_span_Name));
  EXPECT_CALL(callbacks, activeSpan).Times(0);

  transformer.transform(headers, &headers, body, callbacks);
}

const std::string NESTED_KEY =
    R"EOF(
{
  "key":{
    "value": "hello"
  }
}
)EOF";

TEST_F(InjaTransformerTest, ParseFromDynamicMetadata) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;
  transformation.mutable_body()->set_text("{{dynamic_metadata(\"key:value\")}}");

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  ProtobufWkt::Struct struct_obj;
  auto status = ProtobufUtil::JsonStringToMessage(NESTED_KEY, &struct_obj);
  envoy::config::core::v3::Metadata meta;
  meta.mutable_filter_metadata()->insert(
      {SoloHttpFilterNames::get().Transformation,
       struct_obj});
  ON_CALL(callbacks.stream_info_, dynamicMetadata())
      .WillByDefault(testing::ReturnRefOfCopy(meta));

  Buffer::OwnedImpl body("1");
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), "hello");
}

const std::string NESTED_LIST =
    R"EOF(
{
  "key":{
    "value": [
      1,
      2
    ]
  }
}
)EOF";

TEST_F(InjaTransformerTest, ParseFromDynamicMetadataList) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;
  transformation.mutable_body()->set_text("{{dynamic_metadata(\"key:value\")}}");

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  ProtobufWkt::Struct struct_obj;
  auto status = ProtobufUtil::JsonStringToMessage(NESTED_LIST, &struct_obj);
  envoy::config::core::v3::Metadata meta;
  meta.mutable_filter_metadata()->insert(
      {SoloHttpFilterNames::get().Transformation,
       struct_obj});
  ON_CALL(callbacks.stream_info_, dynamicMetadata())
      .WillByDefault(testing::ReturnRefOfCopy(meta));

  Buffer::OwnedImpl body("1");
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), "[1,2]");
}

const std::string INVALID_MATCHER =
    R"EOF(
{
  "key":[
    1,
    2
  ]
}
)EOF";

TEST_F(InjaTransformerTest, ParseFromClusterMetadataList) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;
  transformation.mutable_body()->set_text("{{cluster_metadata(\"key\")}}");

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  ProtobufWkt::Struct struct_obj;
  auto status = ProtobufUtil::JsonStringToMessage(INVALID_MATCHER, &struct_obj);
  envoy::config::core::v3::Metadata meta;
  meta.mutable_filter_metadata()->insert(
      {SoloHttpFilterNames::get().Transformation,
       struct_obj});
  ON_CALL(*callbacks.cluster_info_, metadata())
      .WillByDefault(testing::ReturnRefOfCopy(meta));

  Buffer::OwnedImpl body("1");
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), "[1,2]");
}

TEST_F(InjaTransformerTest, ParseFromClusterMetadataListDeprecated) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;
  transformation.mutable_body()->set_text("{{clusterMetadata(\"key\")}}");

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  ProtobufWkt::Struct struct_obj;
  auto status = ProtobufUtil::JsonStringToMessage(INVALID_MATCHER, &struct_obj);
  envoy::config::core::v3::Metadata meta;
  meta.mutable_filter_metadata()->insert(
      {SoloHttpFilterNames::get().Transformation,
       struct_obj});
  ON_CALL(*callbacks.cluster_info_, metadata())
      .WillByDefault(testing::ReturnRefOfCopy(meta));

  Buffer::OwnedImpl body("1");
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), "1,2");
}

TEST_F(InjaTransformerTest, ParseFromNilClusterInfo) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;
  transformation.mutable_body()->set_text("{{clusterMetadata(\"key\")}}");

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  callbacks.cluster_info_.reset();
  callbacks.cluster_info_ = nullptr;

  Buffer::OwnedImpl body("1");
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), "");
}

TEST_F(TransformerTest, transformHeaderAndHeadersToRemove) {
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
  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);
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

TEST_F(InjaTransformerTest, ReplaceWithRandomBodyTest) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;

  auto pattern = "replace-me";
  auto formatted_string = fmt::format("{{{{replace_with_random(body(), \"{}\")}}}}", pattern);

  transformation.mutable_body()->set_text(formatted_string);
  transformation.set_parse_body_behavior(TransformationTemplate::DontParse);
  transformation.set_advanced_templates(false);

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  Buffer::OwnedImpl body("test-replace-me");
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_TRUE(body.toString().find("test-") != std::string::npos);
  // length of "test-" + 128 bit long random number, Base64-encoded without padding (128/6 ~ 22).
  EXPECT_EQ(27, body.toString().length());
}

TEST_F(InjaTransformerTest, ReplaceWithRandomHeaderTest) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}, {"x-test-123", "abcdef-replace-me"}};
  TransformationTemplate transformation;

  auto pattern = "replace-me";
  auto formatted_string = fmt::format("{{{{replace_with_random(header(\"x-test-123\"), \"{}\")}}}}", pattern);

  (*transformation.mutable_headers())["x-test-123"].set_text(formatted_string);

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  Buffer::OwnedImpl body("");
  transformer.transform(headers, &headers, body, callbacks);

  EXPECT_TRUE(headers.get_("x-test-123").find("abcdef-") != std::string::npos);
  // length of "test-" + 128 bit long random number, Base64-encoded without padding (128/6 ~ 22).
  EXPECT_EQ(29, headers.get_("x-test-123").length());
}

TEST_F(InjaTransformerTest, ReplaceWithRandomTestButNothingToReplace) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;

  auto pattern = "replace-me";
  auto test_string = "nothing-to-replace-here";
  auto formatted_string = fmt::format("{{{{replace_with_random(\"{}\", \"{}\")}}}}", test_string, pattern);

  transformation.mutable_body()->set_text(formatted_string);

  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  Buffer::OwnedImpl body("");
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_STREQ(test_string, body.toString().c_str());
}

void assert_replacements(std::string&& body, std::string&& pattern) {
  auto p1 = body.find(pattern);
  auto p2 = body.rfind(pattern);
  EXPECT_NE(std::string::npos, p1);
  EXPECT_NE(std::string::npos, p2);
  EXPECT_NE(p1, p2);
  // 22 -- the length of a base64 encoded 128 bit long string
  EXPECT_STREQ(body.substr(p1, 22).c_str(), body.substr(p2, 22).c_str());
}

TEST_F(InjaTransformerTest, ReplaceWithRandomTest_SameReplacementPatternUsesSameRandomString) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;

  std::string pattern1{"replace-me"};
  std::string test_string1{"test-1-replace-me"};
  std::string pattern2{"another-replace-me"};
  std::string test_string2{"test-2-another-replace-me"};
  std::string pattern3{"yet-another-replace-me"};
  std::string test_string3{"test-3-yet-another-replace-me"};

  constexpr static char format_string[] =
"{{{{ replace_with_random(\"{}\", \"{}\") }}}}\n"
"{{{{ replace_with_random(\"{}\", \"{}\") }}}}\n"
"{{{{ replace_with_random(\"{}\", \"{}\") }}}}\n"
"{{{{ replace_with_random(\"{}\", \"{}\") }}}}\n"
"{{{{ replace_with_random(\"{}\", \"{}\") }}}}\n"
"{{{{ replace_with_random(\"{}\", \"{}\") }}}}\n";

  auto formatted_string = fmt::format(format_string,
    test_string1, pattern1,
    test_string2, pattern2,
    test_string3, pattern3,
    test_string1, pattern1,
    test_string2, pattern2,
    test_string3, pattern3
    );

  transformation.mutable_body()->set_text(formatted_string);
  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  Buffer::OwnedImpl body("");
  transformer.transform(headers, &headers, body, callbacks);

  assert_replacements(body.toString(), "test-1-");
  assert_replacements(body.toString(), "test-2-");
  assert_replacements(body.toString(), "test-3-");
}

TEST_F(InjaTransformerTest, ParseUsingSetKeyword) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;
  transformation.mutable_body()->set_text(
      "{% set foo = \"bar\" %}{{ foo }}");
  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  Buffer::OwnedImpl body("{\"bat\":\"baz\"}");
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), "bar");
}

TEST_F(InjaTransformerTest, ParseUsingJsonPointerSyntax) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;
  transformation.set_advanced_templates(true);
  transformation.mutable_body()->set_text(
      "{{ json_pointers/example.com }}--{{ json_pointers/and~1or }}--{{ json_pointers/and~0or }}");
  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  Buffer::OwnedImpl body(R"EOF(
{
  "json_pointers": {
    "example.com": "online",
    "and/or": "slash",
    "and~or": "tilde"
  }
}
)EOF");
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), "online--slash--tilde");
}

TEST_F(InjaTransformerTest, EscapeCharacters) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;
  transformation.mutable_body()->set_text(
      R"EOF({"Value":"{{ value }}"})EOF");
  transformation.set_escape_characters(true);
  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  Buffer::OwnedImpl body(R"({"value":"\"foo\""})"_json.dump());
  auto expected_body = R"({"Value":"\"foo\""})"_json.dump();
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), expected_body);
}

TEST_F(InjaTransformerTest, EscapeCharactersNestedJson) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;
  transformation.mutable_body()->set_text(
      R"EOF({"Value":"{{ value }}"})EOF");
  transformation.set_escape_characters(true);
  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  Buffer::OwnedImpl body(R"({"value":"{\"foo\":{\"bar\":\"\\\"baz\\\"\"}}"})"_json.dump());
  auto expected_body = R"({"Value":"{\"foo\":{\"bar\":\"\\\"baz\\\"\"}}"})"_json.dump();
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), expected_body);
}

TEST_F(InjaTransformerTest, EscapeCharactersDoubleEscapedInput) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;
  transformation.mutable_body()->set_text(
      R"EOF({"Value":"{{ value }}"})EOF");
  transformation.set_escape_characters(false);
  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  Buffer::OwnedImpl body(R"({"value":"\\\"foo\\\""})"_json.dump());
  auto expected_body = R"({"Value":"\"foo\""})"_json.dump();
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), expected_body);
}

TEST_F(InjaTransformerTest, RawStringCallback) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;
  transformation.mutable_body()->set_text(
      R"EOF({"Value":"{{ raw_string(value) }}"})EOF");
  transformation.set_escape_characters(false);
  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  Buffer::OwnedImpl body(R"({"value":"\"foo\""})"_json.dump());
  auto expected_body = R"({"Value":"\"foo\""})"_json.dump();
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), expected_body);
}

TEST_F(InjaTransformerTest, RawStringCallbackTooManyArguments) {
  TransformationTemplate transformation;
  transformation.mutable_body()->set_text(
      R"EOF({% set bad="extra argument to function" %}{"Value":"{{ raw_string(value, bad) }}"})EOF");
  EXPECT_THROW_WITH_REGEX(InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_), EnvoyException, ".*unknown function raw_string.*")
}

TEST_F(InjaTransformerTest, RawStringCallbackZeroArguments) {
  TransformationTemplate transformation;
  transformation.mutable_body()->set_text(
      R"EOF({"Value":"{{ raw_string() }}"})EOF");
  EXPECT_THROW_WITH_REGEX(InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_), EnvoyException, ".*unknown function raw_string.*")
}

TEST_F(InjaTransformerTest, EscapeCharactersRawStringCallback) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;
  transformation.mutable_body()->set_text(
      R"EOF({"Value":"{{ raw_string(value) }}"})EOF");
  transformation.set_escape_characters(true);
  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  Buffer::OwnedImpl body(R"({"value":"\"foo\""})"_json.dump());
  auto expected_body = R"({"Value":"\\\"foo\\\""})"_json.dump();
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), expected_body);
}

TEST_F(InjaTransformerTest, DataSourceCallback) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;
	Envoy::Config::DataSource::ProtoDataSource ds;
	ds.set_inline_string("foo");
	transformation.mutable_data_sources()->insert({"ds", ds});
  transformation.mutable_body()->set_text(
      R"EOF({"Value":"{{ data_source("ds") }}"})EOF");
  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  Buffer::OwnedImpl body("{}"_json.dump());
  auto expected_body = R"({"Value":"foo"})"_json.dump();
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), expected_body);
}

TEST_F(InjaTransformerTest, TrimCallback) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  TransformationTemplate transformation;
  transformation.mutable_body()->set_text(
      R"EOF({"Value":"{{ trim(value) }}"})EOF");
  InjaTransformer transformer(transformation, google::protobuf::BoolValue(), factory_context_.dispatcher_, factory_context_.api_, tls_);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  Buffer::OwnedImpl body("{\"value\":\"\\t  foo \\r\\n\"}"_json.dump());
  auto expected_body = R"({"Value":"foo"})"_json.dump();
  transformer.transform(headers, &headers, body, callbacks);
  EXPECT_EQ(body.toString(), expected_body);
}

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
