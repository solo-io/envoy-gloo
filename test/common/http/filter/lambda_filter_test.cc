#include "common/http/filter/lambda_filter.h"

#include "server/config/http/lambda_filter_config_factory.h"

#include "test/mocks/common.h"
#include "test/mocks/lambda/mocks.h"
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

namespace Envoy {
namespace Http {

using Http::LambdaFilterConfig;
using Server::Configuration::LambdaFilterConfigFactory;

// Nothing here as we mock the function retriever
class NothingMetadataAccessor : public MetadataAccessor {
public:
  virtual Optional<const std::string *> getFunctionName() const { return {}; }
  virtual Optional<const ProtobufWkt::Struct *> getFunctionSpec() const {
    return {};
  }
  virtual Optional<const ProtobufWkt::Struct *> getClusterMetadata() const {
    return {};
  }
  virtual Optional<const ProtobufWkt::Struct *> getRouteMetadata() const {
    return {};
  }

  virtual ~NothingMetadataAccessor() {}
};

TEST(LambdaFilterConfigTest, EmptyConfig) {
  envoy::api::v2::filter::http::Lambda config;

  // shouldnt throw.
  LambdaFilterConfig cfg(config);
}

class LambdaFilterTest : public testing::Test {
public:
  LambdaFilterTest() {}

protected:
  void SetUp() override {
    function_retriever_ =
        std::make_shared<NiceMock<Envoy::Http::MockFunctionRetriever>>();
    envoy::api::v2::filter::http::Lambda config;
    config_ = std::make_shared<LambdaFilterConfig>(config);
    filter_ = std::make_unique<LambdaFilter>(config_, function_retriever_);
    filter_->setDecoderFilterCallbacks(filter_callbacks_);
  }

  NiceMock<Envoy::Http::MockStreamDecoderFilterCallbacks> filter_callbacks_;
  NiceMock<Envoy::Server::Configuration::MockFactoryContext> factory_context_;

  LambdaFilterConfigSharedPtr config_;
  std::unique_ptr<LambdaFilter> filter_;
  std::shared_ptr<NiceMock<Envoy::Http::MockFunctionRetriever>>
      function_retriever_;
};

// see:
// https://docs.aws.amazon.com/AmazonS3/latest/API/sig-v4-header-based-auth.html
TEST_F(LambdaFilterTest, SingsOnHeadersEndStream) {
  // const FunctionalFilterBase& filter = static_cast<const
  // FunctionalFilterBase&>(*filter_);
  EXPECT_CALL(*function_retriever_, getFunction(_)).Times(1);

  Envoy::Http::TestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"}};
  ASSERT_EQ(true, filter_->retrieveFunction(NothingMetadataAccessor()));
  EXPECT_EQ(Envoy::Http::FilterHeadersStatus::Continue,
            filter_->decodeHeaders(headers, true));

  // Check aws headers.
  EXPECT_TRUE(headers.has("Authorization"));
}

TEST_F(LambdaFilterTest, SingsOnDataEndStream) {

  EXPECT_CALL(*function_retriever_, getFunction(_)).Times(1);

  Envoy::Http::TestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"}};
  ASSERT_EQ(true, filter_->retrieveFunction(NothingMetadataAccessor()));
  EXPECT_EQ(Envoy::Http::FilterHeadersStatus::StopIteration,
            filter_->decodeHeaders(headers, false));
  EXPECT_FALSE(headers.has("Authorization"));
  Buffer::OwnedImpl data("data");

  EXPECT_EQ(Envoy::Http::FilterDataStatus::Continue,
            filter_->decodeData(data, true));

  EXPECT_TRUE(headers.has("Authorization"));
}

// see: https://docs.aws.amazon.com/lambda/latest/dg/API_Invoke.html
TEST_F(LambdaFilterTest, CorrectFuncCalled) {
  Envoy::Http::TestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"}};
  ASSERT_EQ(true, filter_->retrieveFunction(NothingMetadataAccessor()));
  EXPECT_EQ(Envoy::Http::FilterHeadersStatus::Continue,
            filter_->decodeHeaders(headers, true));

  EXPECT_EQ("/2015-03-31/functions/" + function_retriever_->name_ +
                "/invocations?Qualifier=" + function_retriever_->qualifier_,
            headers.get_(":path"));
}

// see: https://docs.aws.amazon.com/lambda/latest/dg/API_Invoke.html
TEST_F(LambdaFilterTest, FuncWithoutQualifierCalled) {

  EXPECT_CALL(*function_retriever_, getFunction(_))
      .WillRepeatedly(Return(Function{&function_retriever_->name_,
                                      {},
                                      function_retriever_->async_,
                                      &function_retriever_->host_,
                                      &function_retriever_->region_,
                                      &function_retriever_->access_key_,
                                      &function_retriever_->secret_key_}));

  Envoy::Http::TestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"}};

  ASSERT_EQ(true, filter_->retrieveFunction(NothingMetadataAccessor()));
  EXPECT_EQ(Envoy::Http::FilterHeadersStatus::Continue,
            filter_->decodeHeaders(headers, true));

  EXPECT_EQ("/2015-03-31/functions/" + function_retriever_->name_ +
                "/invocations",
            headers.get_(":path"));
}

TEST_F(LambdaFilterTest, FuncWithEmptyQualifierCalled) {
  function_retriever_->qualifier_ = "";
  Envoy::Http::TestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"}};

  ASSERT_EQ(true, filter_->retrieveFunction(NothingMetadataAccessor()));
  EXPECT_EQ(Envoy::Http::FilterHeadersStatus::Continue,
            filter_->decodeHeaders(headers, true));

  EXPECT_EQ("/2015-03-31/functions/" + function_retriever_->name_ +
                "/invocations",
            headers.get_(":path"));
}

TEST_F(LambdaFilterTest, AsyncCalled) {
  Envoy::Http::TestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"}};

  function_retriever_->async_ = true;
  ASSERT_EQ(true, filter_->retrieveFunction(NothingMetadataAccessor()));
  EXPECT_EQ(Envoy::Http::FilterHeadersStatus::Continue,
            filter_->decodeHeaders(headers, true));
  EXPECT_EQ("Event", headers.get_("x-amz-invocation-type"));
}

TEST_F(LambdaFilterTest, SyncCalled) {
  Envoy::Http::TestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"}};

  function_retriever_->async_ = false;
  ASSERT_EQ(true, filter_->retrieveFunction(NothingMetadataAccessor()));
  EXPECT_EQ(Envoy::Http::FilterHeadersStatus::Continue,
            filter_->decodeHeaders(headers, true));
  EXPECT_EQ("RequestResponse", headers.get_("x-amz-invocation-type"));
}

TEST_F(LambdaFilterTest, SignOnTrailedEndStream) {
  EXPECT_CALL(*function_retriever_, getFunction(_)).Times(1);
  Envoy::Http::TestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"}};

  ASSERT_EQ(true, filter_->retrieveFunction(NothingMetadataAccessor()));
  EXPECT_EQ(Envoy::Http::FilterHeadersStatus::StopIteration,
            filter_->decodeHeaders(headers, false));
  Buffer::OwnedImpl data("data");

  EXPECT_EQ(Envoy::Http::FilterDataStatus::StopIterationAndBuffer,
            filter_->decodeData(data, false));
  EXPECT_FALSE(headers.has("Authorization"));

  Envoy::Http::TestHeaderMapImpl trailers;
  EXPECT_EQ(Envoy::Http::FilterTrailersStatus::Continue,
            filter_->decodeTrailers(trailers));

  EXPECT_TRUE(headers.has("Authorization"));
}

TEST_F(LambdaFilterTest, InvalidFunction) {
  // invalid function
  EXPECT_CALL(*function_retriever_, getFunction(_))
      .WillRepeatedly(Return(Optional<Function>()));

  Envoy::Http::TestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"}};

  EXPECT_EQ(false, filter_->retrieveFunction(NothingMetadataAccessor()));
}

} // namespace Http
} // namespace Envoy
