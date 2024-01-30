#include "source/extensions/filters/http/solo_well_known_names.h"
#include "source/extensions/filters/http/transformation/inja_transformer.h"
#include "source/common/common/base64.h"
#include "source/common/common/random_generator.h"

#include "test/mocks/common.h"
#include "test/mocks/http/mocks.h"
#include "test/mocks/server/mocks.h"
#include "test/mocks/thread_local/mocks.h"
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
  NiceMock<Random::MockRandomGenerator> rng_;
  NiceMock<ThreadLocal::MockInstance> tls_;
};

void fill_slot(
      ThreadLocal::SlotPtr& slot,
      const Http::RequestOrResponseHeaderMap &header_map,
      const Http::RequestHeaderMap *request_headers,
      GetBodyFunc &body,
      const std::unordered_map<std::string, absl::string_view> &extractions,
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
  typed_slot.context_ = &context;
  typed_slot.environ_ = &environ;
  typed_slot.cluster_metadata_ = cluster_metadata;
}

TEST(Extraction, ExtractAndReplaceValueFromBodySubgroup) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};

  envoy::api::v2::filter::http::Extraction extractor;
  extractor.mutable_body();
  extractor.set_regex(".*(body)");
  extractor.set_subgroup(1);
  auto replacement_text = "BAZ";
  extractor.mutable_replacement_text()->set_value(replacement_text);
  extractor.set_mode(envoy::api::v2::filter::http::Extraction::SINGLE_REPLACE);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("not json body");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };
  std::string res(Extractor(extractor).extract(callbacks, headers, bodyfunc));

  EXPECT_EQ("not json BAZ", res);
}

TEST(Extraction, ExtractAndReplaceValueFromFullBody) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};

  envoy::api::v2::filter::http::Extraction extractor;
  extractor.mutable_body();
  extractor.set_regex(".*");
  extractor.set_subgroup(0);
  auto replacement_text = "BAZ";
  extractor.mutable_replacement_text()->set_value(replacement_text);
  extractor.set_mode(envoy::api::v2::filter::http::Extraction::SINGLE_REPLACE);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("not json body");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };
  std::string res(Extractor(extractor).extract(callbacks, headers, bodyfunc));

  EXPECT_EQ("BAZ", res);
}

// TODO: EDGE CASE !!!
TEST(Extraction, ExtractAndReplaceAllFromFullBody) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};

  envoy::api::v2::filter::http::Extraction extractor;
  extractor.mutable_body();
  extractor.set_regex(".*");
  extractor.set_subgroup(0);
  auto replacement_text = "BAZ";
  extractor.mutable_replacement_text()->set_value(replacement_text);
  extractor.set_mode(envoy::api::v2::filter::http::Extraction::REPLACE_ALL);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("not json body");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };
  std::string res(Extractor(extractor).extract(callbacks, headers, bodyfunc));

  // Note to self/reviewers: this is the current behavior, which is a kind of 
  // confusing edge case in std::regex_replace when the regex is .*
  // apparently, this regex matches the whole input string __AND__ the 
  // line ending, so the replacement is applied twice
  EXPECT_EQ("BAZBAZ", res);
}

TEST(Extraction, AttemptReplaceFromPartialMatch) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};

  envoy::api::v2::filter::http::Extraction extractor;
  extractor.mutable_body();
  // Unless we are in `REPLACE_ALL` mode, we require regexes to match the entire target string
  // because this only matches a substring, it should not be replaced
  extractor.set_regex("body");
  extractor.set_subgroup(0);
  auto replacement_text = "BAZ";
	extractor.mutable_replacement_text()->set_value(replacement_text);
  extractor.set_mode(envoy::api::v2::filter::http::Extraction::SINGLE_REPLACE);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("not json body");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };
  std::string res(Extractor(extractor).extract(callbacks, headers, bodyfunc));

  EXPECT_EQ("", res);
}

TEST(Extraction, AttemptReplaceFromPartialMatchNonNilSubgroup) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};

  envoy::api::v2::filter::http::Extraction extractor;
  extractor.mutable_body();
  // Unless we are in `REPLACE_ALL` mode, we require regexes to match the entire target string
  // because this only matches a substring, it should not be replaced
  // Note -- the subgroup in the regex is introduced here so that this config is not
  // rejected when constructing the extractor
  extractor.set_regex("(body)");
  extractor.set_subgroup(1);
  auto replacement_text = "BAZ";
	extractor.mutable_replacement_text()->set_value(replacement_text);
  extractor.set_mode(envoy::api::v2::filter::http::Extraction::SINGLE_REPLACE);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("not json body");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };
  std::string res(Extractor(extractor).extract(callbacks, headers, bodyfunc));

  EXPECT_EQ("", res);
}

TEST(Extraction, AttemptReplaceFromNoMatchNonNilSubgroup) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};

  envoy::api::v2::filter::http::Extraction extractor;
  extractor.mutable_body();
  // Unless we are in `REPLACE_ALL` mode, we require regexes to match the entire target string
  // because this only matches a substring, it should not be replaced
  // Note -- the subgroup in the regex is introduced here so that this config is not
  // rejected when constructing the extractor
  extractor.set_regex("(does not match)");
  extractor.set_subgroup(1);
  auto replacement_text = "BAZ";
	extractor.mutable_replacement_text()->set_value(replacement_text);
  extractor.set_mode(envoy::api::v2::filter::http::Extraction::SINGLE_REPLACE);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("not json body");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };
  std::string res(Extractor(extractor).extract(callbacks, headers, bodyfunc));

  EXPECT_EQ("", res);
}


TEST(Extraction, ReplaceFromFullLiteralMatch) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};

  envoy::api::v2::filter::http::Extraction extractor;
  extractor.mutable_body();
  // We require regexes to match the entire target string
  // because this only matches a substring, it should not be replaced
  extractor.set_regex("not json body");
  extractor.set_subgroup(0);
  auto replacement_text = "BAZ";
	extractor.mutable_replacement_text()->set_value(replacement_text);
  extractor.set_mode(envoy::api::v2::filter::http::Extraction::SINGLE_REPLACE);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("not json body");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };
  std::string res(Extractor(extractor).extract(callbacks, headers, bodyfunc));

  EXPECT_EQ("BAZ", res);
}

TEST(Extraction, AttemptToReplaceFromInvalidSubgroup) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};

  envoy::api::v2::filter::http::Extraction extractor;
  extractor.mutable_body();
  extractor.set_regex(".*");
  extractor.set_subgroup(1);
  auto replacement_text = "BAZ";
	extractor.mutable_replacement_text()->set_value(replacement_text);
  extractor.set_mode(envoy::api::v2::filter::http::Extraction::SINGLE_REPLACE);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("not json body");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };
  EXPECT_THROW_WITH_MESSAGE(Extractor(extractor).extract(callbacks, headers, bodyfunc), EnvoyException, "group 1 requested for regex with only 0 sub groups");
}

TEST(Extraction, ReplaceInNestedSubgroups) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};

  envoy::api::v2::filter::http::Extraction extractor;
  extractor.mutable_body();
  extractor.set_regex(".*(not (json) body)");
  extractor.set_subgroup(2);
  auto replacement_text = "BAZ";
	extractor.mutable_replacement_text()->set_value(replacement_text);
  extractor.set_mode(envoy::api::v2::filter::http::Extraction::SINGLE_REPLACE);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("not json body");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };
  std::string res(Extractor(extractor).extract(callbacks, headers, bodyfunc));

  EXPECT_EQ("not BAZ body", res);
}

TEST(Extraction, ReplaceWithSubgroupUnset) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};

  envoy::api::v2::filter::http::Extraction extractor;
  extractor.mutable_body();
  extractor.set_regex(".*(not (json) body)");
  // subgroup is unset
  auto replacement_text = "BAZ";
	extractor.mutable_replacement_text()->set_value(replacement_text);
  extractor.set_mode(envoy::api::v2::filter::http::Extraction::SINGLE_REPLACE);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("not json body");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };
  std::string res(Extractor(extractor).extract(callbacks, headers, bodyfunc));

  EXPECT_EQ("BAZ", res);
}

// In regular extractor, I expect that this will hit the "this should never happen" block
TEST(Extraction, ReplaceNoMatch) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};

  envoy::api::v2::filter::http::Extraction extractor;
  extractor.mutable_body();
  extractor.set_regex("this will not match the input string");
  extractor.set_subgroup(0);
  auto replacement_text = "BAZ";
	extractor.mutable_replacement_text()->set_value(replacement_text);
  extractor.set_mode(envoy::api::v2::filter::http::Extraction::SINGLE_REPLACE);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("not json body");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };
  std::string res(Extractor(extractor).extract(callbacks, headers, bodyfunc));

  EXPECT_EQ("", res);
}

TEST(Extraction, NilReplace) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};

  envoy::api::v2::filter::http::Extraction extractor;
  extractor.mutable_body();
  extractor.set_regex(".*(body)");
  extractor.set_subgroup(1);
  auto replacement_text = "";
	extractor.mutable_replacement_text()->set_value(replacement_text);
  extractor.set_mode(envoy::api::v2::filter::http::Extraction::SINGLE_REPLACE);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("not json body");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };
  std::string res(Extractor(extractor).extract(callbacks, headers, bodyfunc));

  EXPECT_EQ("not json ", res);
}

TEST(Extraction, NilReplaceWithSubgroupUnset) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};

  // subgroup is unset
  envoy::api::v2::filter::http::Extraction extractor;
  extractor.mutable_body();
  extractor.set_regex(".*(body)");
  auto replacement_text = "";
  extractor.mutable_replacement_text()->set_value(replacement_text);
  extractor.set_mode(envoy::api::v2::filter::http::Extraction::SINGLE_REPLACE);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("not json body");
  std::string res(Extractor(extractor).extract(callbacks, headers, empty_body));

  EXPECT_EQ("", res);
}

TEST(Extraction, HeaderReplaceHappyPath) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}, {"foo", "bar"}};

  envoy::api::v2::filter::http::Extraction extractor;
  extractor.set_header("foo");
  extractor.set_regex("bar");
  extractor.set_subgroup(0);
  auto replacement_text = "BAZ";
	extractor.mutable_replacement_text()->set_value(replacement_text);
  extractor.set_mode(envoy::api::v2::filter::http::Extraction::SINGLE_REPLACE);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("not json body");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };

  std::string res(Extractor(extractor).extract(callbacks, headers, bodyfunc));

  EXPECT_EQ("BAZ", res);
}

TEST(Extraction, ReplaceAllWithReplacementTextUnset) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}, {"foo", "bar"}};

  envoy::api::v2::filter::http::Extraction extractor;
  extractor.mutable_body();
  extractor.set_regex("bar");
  extractor.set_subgroup(0);
  extractor.set_mode(envoy::api::v2::filter::http::Extraction::REPLACE_ALL);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("bar bar bar");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };

  EXPECT_THROW(std::string res(Extractor(extractor).extract(callbacks, headers, bodyfunc)), EnvoyException);
}

TEST(Extraction, ReplaceAllWithSubgroupSet) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}, {"foo", "bar"}};

  envoy::api::v2::filter::http::Extraction extractor;
  extractor.mutable_body();
  extractor.set_regex(".*(bar).*");
  // Note that the regex contains enough capture groups
  // that this (in theory) could be valid subgroup
  extractor.set_subgroup(1);
  auto replacement_text = "BAZ";
	extractor.mutable_replacement_text()->set_value(replacement_text);
  // However, subgroup needs to be unset (i.e., 0) for replace all to work
  // so this config should be rejected
  extractor.set_mode(envoy::api::v2::filter::http::Extraction::REPLACE_ALL);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("bar bar bar");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };

  EXPECT_THROW(std::string res(Extractor(extractor).extract(callbacks, headers, bodyfunc)), EnvoyException);
}

TEST(Extraction, ReplaceAllHappyPath) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}, {"foo", "bar"}};

  envoy::api::v2::filter::http::Extraction extractor;
  extractor.mutable_body();
  extractor.set_regex("bar");
  extractor.set_subgroup(0);
  auto replacement_text = "BAZ";
  extractor.mutable_replacement_text()->set_value(replacement_text);
  extractor.set_mode(envoy::api::v2::filter::http::Extraction::REPLACE_ALL);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("bar bar bar");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };

  std::string res(Extractor(extractor).extract(callbacks, headers, bodyfunc));

  EXPECT_EQ("BAZ BAZ BAZ", res);
}

TEST(Extraction, IndividualReplaceIdentity) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}, {"foo", "bar"}};

  // Note that the regex contains enough capture groups
  // that this (in theory) could be valid subgroup
  envoy::api::v2::filter::http::Extraction extractor;
  extractor.mutable_body();
  extractor.set_regex(".*(bar).*");
  extractor.set_subgroup(1);
  auto replacement_text = "bar";
  extractor.mutable_replacement_text()->set_value(replacement_text);
  extractor.set_mode(envoy::api::v2::filter::http::Extraction::SINGLE_REPLACE);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("bar bar bar");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };

  std::string res(Extractor(extractor).extract(callbacks, headers, bodyfunc));

  EXPECT_EQ("bar bar bar", res);
}

TEST(Extraction, ReplaceAllIdentity) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}, {"foo", "bar"}};

  envoy::api::v2::filter::http::Extraction extractor;
  extractor.mutable_body();
  extractor.set_regex("bar");
  extractor.set_subgroup(0);
  auto replacement_text = "bar";
  extractor.mutable_replacement_text()->set_value(replacement_text);
  extractor.set_mode(envoy::api::v2::filter::http::Extraction::REPLACE_ALL);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("bar bar bar");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };

  std::string res(Extractor(extractor).extract(callbacks, headers, bodyfunc));

  EXPECT_EQ("bar bar bar", res);
}

TEST(Extraction, ReplaceAllNoMatch) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}, {"foo", "bar"}};

  // Note that the regex contains enough capture groups
  // that this (in theory) could be valid subgroup
  envoy::api::v2::filter::http::Extraction extractor;
  extractor.mutable_body();
  extractor.set_regex("this will not match the input string");
  extractor.set_subgroup(0);
  auto replacement_text = "BAZ";
  extractor.mutable_replacement_text()->set_value(replacement_text);
  extractor.set_mode(envoy::api::v2::filter::http::Extraction::REPLACE_ALL);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("not json body");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };

  std::string res(Extractor(extractor).extract(callbacks, headers, bodyfunc));

  EXPECT_EQ("", res);
}

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
