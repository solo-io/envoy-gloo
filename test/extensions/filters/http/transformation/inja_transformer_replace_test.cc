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

TEST(Extraction, ExtractAndReplaceValueFromBodySubgroup) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};

  ExtractionApi extractor;
  extractor.mutable_body();
  extractor.set_regex(".*(body)");
  extractor.set_subgroup(1);
  extractor.mutable_replacement_text()->set_value("BAZ");
  extractor.set_mode(ExtractionApi::SINGLE_REPLACE);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("not json body");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };
  std::string res(Extractor(extractor).extractDestructive(callbacks, headers, bodyfunc));

  EXPECT_EQ("not json BAZ", res);
}

TEST(Extraction, ExtractAndReplaceValueFromFullBody) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};
  ExtractionApi extractor;
  extractor.mutable_body();
  extractor.set_regex(".*");
  extractor.set_subgroup(0);
  extractor.mutable_replacement_text()->set_value("BAZ");
  extractor.set_mode(ExtractionApi::SINGLE_REPLACE);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("not json body");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };
  std::string res(Extractor(extractor).extractDestructive(callbacks, headers, bodyfunc));

  EXPECT_EQ("BAZ", res);
}

// Note to maintainers: if we don't use the `match_not_null` format specifier
// when calling std::regex_replace, this regex will match the input string twice
// and the replacement will be applied twice. Because we are using the `match_not_null`
// format specifier, the regex will only match the input string once and the replacement
// will only be applied once.
TEST(Extraction, ExtractAndReplaceAllFromFullBody) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};

  ExtractionApi extractor;
  extractor.mutable_body();
  extractor.set_regex(".*");
  extractor.set_subgroup(0);
  extractor.mutable_replacement_text()->set_value("BAZ");
  extractor.set_mode(ExtractionApi::REPLACE_ALL);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("not json body");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };
  std::string res(Extractor(extractor).extractDestructive(callbacks, headers, bodyfunc));

  EXPECT_EQ("BAZ", res);
}

TEST(Extraction, AttemptReplaceFromPartialMatch) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};

  ExtractionApi extractor;
  extractor.mutable_body();
  // Unless we are in `REPLACE_ALL` mode, we require regexes to match the entire target string
  // because this only matches a substring, it should not be replaced
  extractor.set_regex("body");
  extractor.set_subgroup(0);
  extractor.mutable_replacement_text()->set_value("BAZ");
  extractor.set_mode(ExtractionApi::SINGLE_REPLACE);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("not json body");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };
  std::string res(Extractor(extractor).extractDestructive(callbacks, headers, bodyfunc));

  EXPECT_EQ("", res);
}

TEST(Extraction, AttemptReplaceFromPartialMatchNonNilSubgroup) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};

  ExtractionApi extractor;
  extractor.mutable_body();
  // Unless we are in `REPLACE_ALL` mode, we require regexes to match the entire target string
  // because this only matches a substring, it should not be replaced
  // Note -- the subgroup in the regex is introduced here so that this config is not
  // rejected when constructing the extractor
  extractor.set_regex("(body)");
  extractor.set_subgroup(1);
  extractor.mutable_replacement_text()->set_value("BAZ");
  extractor.set_mode(ExtractionApi::SINGLE_REPLACE);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("not json body");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };
  std::string res(Extractor(extractor).extractDestructive(callbacks, headers, bodyfunc));

  EXPECT_EQ("", res);
}

TEST(Extraction, AttemptReplaceFromNoMatchNonNilSubgroup) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};

  ExtractionApi extractor;
  extractor.mutable_body();
  extractor.set_regex("(does not match)");
  extractor.set_subgroup(1);
  extractor.mutable_replacement_text()->set_value("BAZ");
  extractor.set_mode(ExtractionApi::SINGLE_REPLACE);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("not json body");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };
  std::string res(Extractor(extractor).extractDestructive(callbacks, headers, bodyfunc));

  EXPECT_EQ("", res);
}

TEST(Extraction, ReplaceFromFullLiteralMatch) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};

  ExtractionApi extractor;
  extractor.mutable_body();
  extractor.set_regex("not json body");
  extractor.set_subgroup(0);
  extractor.mutable_replacement_text()->set_value("BAZ");
  extractor.set_mode(ExtractionApi::SINGLE_REPLACE);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("not json body");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };
  std::string res(Extractor(extractor).extractDestructive(callbacks, headers, bodyfunc));

  EXPECT_EQ("BAZ", res);
}

TEST(Extraction, AttemptToReplaceFromInvalidSubgroup) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};

  ExtractionApi extractor;
  extractor.mutable_body();
  extractor.set_regex(".*");
  extractor.set_subgroup(1);
  extractor.mutable_replacement_text()->set_value("BAZ");
  extractor.set_mode(ExtractionApi::SINGLE_REPLACE);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("not json body");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };
  EXPECT_THROW_WITH_MESSAGE(Extractor(extractor).extractDestructive(callbacks, headers, bodyfunc), EnvoyException, "group 1 requested for regex with only 0 sub groups");
}

TEST(Extraction, ReplaceInNestedSubgroups) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};

  ExtractionApi extractor;
  extractor.mutable_body();
  extractor.set_regex(".*(not (json) body)");
  extractor.set_subgroup(2);
  auto replacement_text = "BAZ";
	extractor.mutable_replacement_text()->set_value(replacement_text);
  extractor.set_mode(ExtractionApi::SINGLE_REPLACE);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("not json body");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };
  std::string res(Extractor(extractor).extractDestructive(callbacks, headers, bodyfunc));

  EXPECT_EQ("not BAZ body", res);
}

TEST(Extraction, ReplaceWithSubgroupUnset) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};

  ExtractionApi extractor;
  extractor.mutable_body();
  extractor.set_regex(".*(not (json) body)");
  // subgroup is unset
  auto replacement_text = "BAZ";
	extractor.mutable_replacement_text()->set_value(replacement_text);
  extractor.set_mode(ExtractionApi::SINGLE_REPLACE);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("not json body");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };
  std::string res(Extractor(extractor).extractDestructive(callbacks, headers, bodyfunc));

  EXPECT_EQ("BAZ", res);
}

TEST(Extraction, ReplaceNoMatch) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};

  ExtractionApi extractor;
  extractor.mutable_body();
  extractor.set_regex("this will not match the input string");
  extractor.set_subgroup(0);
  extractor.mutable_replacement_text()->set_value("BAZ");
  extractor.set_mode(ExtractionApi::SINGLE_REPLACE);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("not json body");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };
  std::string res(Extractor(extractor).extractDestructive(callbacks, headers, bodyfunc));

  EXPECT_EQ("", res);
}

TEST(Extraction, ReplacementTextLongerThanOriginalString) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};

  ExtractionApi extractor;
  extractor.mutable_body();
  extractor.set_regex(".*(body)");
  extractor.set_subgroup(1);
  extractor.mutable_replacement_text()->set_value("this is a longer string than the original");
  extractor.set_mode(ExtractionApi::SINGLE_REPLACE);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("not json body");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };
  std::string res(Extractor(extractor).extractDestructive(callbacks, headers, bodyfunc));

  EXPECT_EQ("not json this is a longer string than the original", res);
}

TEST(Extraction, NilReplace) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};

  ExtractionApi extractor;
  extractor.mutable_body();
  extractor.set_regex(".*(body)");
  extractor.set_subgroup(1);
  extractor.mutable_replacement_text()->set_value("");
  extractor.set_mode(ExtractionApi::SINGLE_REPLACE);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("not json body");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };
  std::string res(Extractor(extractor).extractDestructive(callbacks, headers, bodyfunc));

  EXPECT_EQ("not json ", res);
}

TEST(Extraction, NilReplaceWithSubgroupUnset) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}};

  // subgroup is unset
  ExtractionApi extractor;
  extractor.mutable_body();
  extractor.set_regex(".*(body)");
  extractor.mutable_replacement_text()->set_value("");
  extractor.set_mode(ExtractionApi::SINGLE_REPLACE);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("not json body");
  std::string res(Extractor(extractor).extractDestructive(callbacks, headers, empty_body));

  EXPECT_EQ("", res);
}

TEST(Extraction, HeaderReplaceHappyPath) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}, {"foo", "bar"}};

  ExtractionApi extractor;
  extractor.set_header("foo");
  extractor.set_regex("bar");
  extractor.set_subgroup(0);
  extractor.mutable_replacement_text()->set_value("BAZ");
  extractor.set_mode(ExtractionApi::SINGLE_REPLACE);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("not json body");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };

  std::string res(Extractor(extractor).extractDestructive(callbacks, headers, bodyfunc));

  EXPECT_EQ("BAZ", res);
}

TEST(Extraction, ReplaceAllWithReplacementTextUnset) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}, {"foo", "bar"}};

  ExtractionApi extractor;
  extractor.mutable_body();
  extractor.set_regex("bar");
  extractor.set_subgroup(0);
  extractor.set_mode(ExtractionApi::REPLACE_ALL);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("bar bar bar");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };

  EXPECT_THROW(std::string res(Extractor(extractor).extractDestructive(callbacks, headers, bodyfunc)), EnvoyException);
}

TEST(Extraction, ReplaceAllWithSubgroupSet) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}, {"foo", "bar"}};

  ExtractionApi extractor;
  extractor.mutable_body();
  extractor.set_regex(".*(bar).*");
  // Note that the regex contains enough capture groups
  // that this (in theory) could be valid subgroup
  extractor.set_subgroup(1);
  extractor.mutable_replacement_text()->set_value("BAZ");
  // However, subgroup needs to be unset (i.e., 0) for replace all to work
  // so this config should be rejected
  extractor.set_mode(ExtractionApi::REPLACE_ALL);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("bar bar bar");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };

  EXPECT_THROW(std::string res(Extractor(extractor).extractDestructive(callbacks, headers, bodyfunc)), EnvoyException);
}

TEST(Extraction, ReplaceAllHappyPath) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}, {"foo", "bar"}};

  ExtractionApi extractor;
  extractor.mutable_body();
  extractor.set_regex("bar");
  extractor.set_subgroup(0);
  extractor.mutable_replacement_text()->set_value("BAZ");
  extractor.set_mode(ExtractionApi::REPLACE_ALL);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("bar bar bar");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };

  std::string res(Extractor(extractor).extractDestructive(callbacks, headers, bodyfunc));

  EXPECT_EQ("BAZ BAZ BAZ", res);
}

TEST(Extraction, IndividualReplaceIdentity) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}, {"foo", "bar"}};

  ExtractionApi extractor;
  extractor.mutable_body();
  extractor.set_regex(".*(bar).*");
  extractor.set_subgroup(1);
  extractor.mutable_replacement_text()->set_value("bar");
  extractor.set_mode(ExtractionApi::SINGLE_REPLACE);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("bar bar bar");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };

  std::string res(Extractor(extractor).extractDestructive(callbacks, headers, bodyfunc));

  EXPECT_EQ("bar bar bar", res);
}

TEST(Extraction, ReplaceAllIdentity) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}, {"foo", "bar"}};

  ExtractionApi extractor;
  extractor.mutable_body();
  extractor.set_regex("bar");
  extractor.set_subgroup(0);
  extractor.mutable_replacement_text()->set_value("bar");
  extractor.set_mode(ExtractionApi::REPLACE_ALL);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("bar bar bar");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };

  std::string res(Extractor(extractor).extractDestructive(callbacks, headers, bodyfunc));

  EXPECT_EQ("bar bar bar", res);
}

TEST(Extraction, ReplaceAllNoMatch) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}, {"foo", "bar"}};

  ExtractionApi extractor;
  extractor.mutable_body();
  extractor.set_regex("this will not match the input string");
  extractor.set_subgroup(0);
  extractor.mutable_replacement_text()->set_value("BAZ");
  extractor.set_mode(ExtractionApi::REPLACE_ALL);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("not json body");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };

  std::string res(Extractor(extractor).extractDestructive(callbacks, headers, bodyfunc));

  EXPECT_EQ("not json body", res);
}

TEST(Extraction, ReplaceAllCapture) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"}, {":path", "/foo"}, {"foo", "bar"}};

  ExtractionApi extractor;
  extractor.mutable_body();
  extractor.set_regex("(not) (json) (body)");
  extractor.set_subgroup(0);
  extractor.mutable_replacement_text()->set_value("$2 $3");
  extractor.set_mode(ExtractionApi::REPLACE_ALL);

  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;
  std::string body("not json body");
  GetBodyFunc bodyfunc = [&body]() -> const std::string & { return body; };

  std::string res(Extractor(extractor).extractDestructive(callbacks, headers, bodyfunc));

  EXPECT_EQ("json body", res);
}

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
