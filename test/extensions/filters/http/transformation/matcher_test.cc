#include "source/extensions/filters/http/transformation/matcher.h"

#include "fmt/format.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test/mocks/server/mocks.h"
#include "test/mocks/stream_info/mocks.h"
#include "test/test_common/utility.h"

using testing::_;
using testing::AtLeast;
using testing::Invoke;
using testing::Return;
using testing::ReturnPointee;
using testing::ReturnRef;
using testing::SaveArg;
using testing::Throw;
using testing::WithArg;

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

TransformerPairConstSharedPtr getFromMatcher(std::string s){
  NiceMock<Server::Configuration::MockServerFactoryContext>
      server_factory_context;
  NiceMock<StreamInfo::MockStreamInfo> stream_info;

  xds::type::matcher::v3::Matcher matcher;
  TestUtility::loadFromYaml(s, matcher);

  auto m = createTransformationMatcher(matcher, server_factory_context);

  Http::Matching::HttpMatchingDataImpl data(stream_info);

  Http::TestRequestHeaderMapImpl headers{
      {":method", "GET"}, {":authority", "www.solo.io"}, {":path", "/path"}};
  data.onRequestHeaders(headers);
  return matchTransform(std::move(data), m);
}

TEST(TransformationMatcher, TestGetRequestTransformer) {
  auto t = getFromMatcher( R"EOF(
    matcher_list:
      matchers:
      - predicate:
          single_predicate:
            input:
              name: envoy.matching.inputs.request_headers
              typed_config:
                "@type": "type.googleapis.com/envoy.type.matcher.v3.HttpRequestHeaderMatchInput"
                header_name: ":path"
            value_match:
              prefix: "/"
        on_match:
          action:
            name: action
            typed_config:
              "@type": "type.googleapis.com/envoy.api.v2.filter.http.TransformationRule.Transformations"
              request_transformation:
                transformation_template:
                  passthrough: {}
                  headers:
                    "x-foo": {text: "matcher"}
  )EOF");

  EXPECT_NE(t, nullptr);
  EXPECT_NE(t->getRequestTranformation(), nullptr);
  EXPECT_EQ(t->getResponseTranformation(), nullptr);
  EXPECT_EQ(t->getOnStreamCompletionTransformation(), nullptr);
  EXPECT_EQ(t->shouldClearCache(), false);
}

TEST(TransformationMatcher, TestGetRequestTransformerClearCache) {
  auto t = getFromMatcher( R"EOF(
    matcher_list:
      matchers:
      - predicate:
          single_predicate:
            input:
              name: envoy.matching.inputs.request_headers
              typed_config:
                "@type": "type.googleapis.com/envoy.type.matcher.v3.HttpRequestHeaderMatchInput"
                header_name: ":path"
            value_match:
              prefix: "/"
        on_match:
          action:
            name: action
            typed_config:
              "@type": "type.googleapis.com/envoy.api.v2.filter.http.TransformationRule.Transformations"
              clear_route_cache: true
              request_transformation:
                transformation_template:
                  passthrough: {}
                  headers:
                    "x-foo": {text: "matcher"}
  )EOF");

  EXPECT_NE(t, nullptr);
  EXPECT_NE(t->getRequestTranformation(), nullptr);
  EXPECT_EQ(t->getResponseTranformation(), nullptr);
  EXPECT_EQ(t->getOnStreamCompletionTransformation(), nullptr);
  EXPECT_EQ(t->shouldClearCache(), true);
}

TEST(TransformationMatcher, TestGetResponseTransformer) {
  auto t = getFromMatcher( R"EOF(
    matcher_list:
      matchers:
      - predicate:
          single_predicate:
            input:
              name: envoy.matching.inputs.request_headers
              typed_config:
                "@type": "type.googleapis.com/envoy.type.matcher.v3.HttpRequestHeaderMatchInput"
                header_name: ":path"
            value_match:
              prefix: "/"
        on_match:
          action:
            name: action
            typed_config:
              "@type": "type.googleapis.com/envoy.api.v2.filter.http.TransformationRule.Transformations"
              response_transformation:
                 transformation_template:
                   passthrough: {}
                   headers:
                     "x-foo": {text: "matcher"}
  )EOF");

  EXPECT_NE(t, nullptr);
  EXPECT_EQ(t->getRequestTranformation(), nullptr);
  EXPECT_NE(t->getResponseTranformation(), nullptr);
  EXPECT_EQ(t->getOnStreamCompletionTransformation(), nullptr);
  EXPECT_EQ(t->shouldClearCache(), false);
}

TEST(TransformationMatcher, TestGetStreamCompleteTransformer) {
  auto t = getFromMatcher( R"EOF(
    matcher_list:
      matchers:
      - predicate:
          single_predicate:
            input:
              name: envoy.matching.inputs.request_headers
              typed_config:
                "@type": "type.googleapis.com/envoy.type.matcher.v3.HttpRequestHeaderMatchInput"
                header_name: ":path"
            value_match:
              prefix: "/"
        on_match:
          action:
            name: action
            typed_config:
              "@type": "type.googleapis.com/envoy.api.v2.filter.http.TransformationRule.Transformations"
              on_stream_completion_transformation:
                 transformation_template:
                   passthrough: {}
                   headers:
                     "x-foo": {text: "matcher"}
  )EOF");

  EXPECT_NE(t, nullptr);
  EXPECT_EQ(t->getRequestTranformation(), nullptr);
  EXPECT_EQ(t->getResponseTranformation(), nullptr);
  EXPECT_NE(t->getOnStreamCompletionTransformation(), nullptr);
  EXPECT_EQ(t->shouldClearCache(), false);
}

TEST(TransformationMatcher, TestGetTransformOnNonMatch) {
  auto t = getFromMatcher( R"EOF(
    matcher_list:
      matchers:
      - predicate:
          single_predicate:
            input:
              name: envoy.matching.inputs.request_headers
              typed_config:
                "@type": "type.googleapis.com/envoy.type.matcher.v3.HttpRequestHeaderMatchInput"
                header_name: ":path"
            value_match:
              prefix: "/prefix_no_exists"
        on_match:
          action:
            name: action
            typed_config:
              "@type": "type.googleapis.com/envoy.api.v2.filter.http.TransformationRule.Transformations"
              on_stream_completion_transformation:
                transformation_template:
                  passthrough: {}
                  headers:
                    "x-foo": {text: "matcher"}
    on_no_match:
      action:
        name: action
        typed_config:
          "@type": "type.googleapis.com/envoy.api.v2.filter.http.TransformationRule.Transformations"
          request_transformation:
             transformation_template:
               passthrough: {}
               headers:
                 "x-foo": {text: "matcher"}
  )EOF");

  EXPECT_NE(t, nullptr);
  EXPECT_NE(t->getRequestTranformation(), nullptr);
  EXPECT_EQ(t->getResponseTranformation(), nullptr);
  EXPECT_EQ(t->getOnStreamCompletionTransformation(), nullptr);
  EXPECT_EQ(t->shouldClearCache(), false);
}

}  // namespace Transformation
}  // namespace HttpFilters
}  // namespace Extensions
}  // namespace Envoy