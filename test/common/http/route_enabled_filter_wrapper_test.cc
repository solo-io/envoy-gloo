#include <iostream>

#include "common/http/route_enabled_filter_wrapper.h"
#include "common/protobuf/utility.h"
#include "common/router/config_impl.h"

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

namespace Envoy {
namespace Http {

class WrapperFilterTest;

class WrapperFilterTester : public StreamDecoderFilter {
public:
  WrapperFilterTester(WrapperFilterTest &testfixture)
      : testfixture_(testfixture) {}

  virtual FilterHeadersStatus decodeHeaders(HeaderMap &, bool) override;
  virtual FilterDataStatus decodeData(Buffer::Instance &, bool) override;
  virtual FilterTrailersStatus decodeTrailers(HeaderMap &) override;

  void onDestroy() override {}

  void setDecoderFilterCallbacks(StreamDecoderFilterCallbacks &) override {}

  WrapperFilterTest &testfixture_;
};

class WrapperFilterTest : public testing::Test {
public:
  WrapperFilterTest()
      : functionDecodeHeadersCalled_(false), functionDecodeDataCalled_(false),
        functionDecodeTrailersCalled_(false), childname_("childfilter") {}

  bool functionDecodeHeadersCalled_;
  bool functionDecodeDataCalled_;
  bool functionDecodeTrailersCalled_;

protected:
  void SetUp() override {
    initFilter();

    Router::MockRouteEntry &routerentry =
        filter_callbacks_.route_->route_entry_;
    ON_CALL(routerentry, metadata()).WillByDefault(ReturnRef(route_metadata_));
  }

  void initFilter() {
    filter_ = std::make_unique<RouteEnabledFilterWrapper<WrapperFilterTester>>(
        childname_, *this);
    filter_->setDecoderFilterCallbacks(filter_callbacks_);
  }

  void initchildroutemeta() {
    // mark the route as active
    ProtobufWkt::Struct routefunctionmeta;
    (*route_metadata_.mutable_filter_metadata())[childname_] =
        routefunctionmeta;
  }

  NiceMock<MockStreamDecoderFilterCallbacks> filter_callbacks_;
  NiceMock<Server::Configuration::MockFactoryContext> factory_context_;
  std::unique_ptr<RouteEnabledFilterWrapper<WrapperFilterTester>> filter_;
  std::string childname_;
  envoy::api::v2::core::Metadata route_metadata_;
};

FilterHeadersStatus WrapperFilterTester::decodeHeaders(HeaderMap &, bool) {
  testfixture_.functionDecodeHeadersCalled_ = true;
  return FilterHeadersStatus::Continue;
}

FilterDataStatus WrapperFilterTester::decodeData(Buffer::Instance &, bool) {
  testfixture_.functionDecodeDataCalled_ = true;
  return FilterDataStatus::Continue;
}

FilterTrailersStatus WrapperFilterTester::decodeTrailers(HeaderMap &) {
  testfixture_.functionDecodeTrailersCalled_ = true;
  return FilterTrailersStatus::Continue;
}

TEST_F(WrapperFilterTest, NothingConfigured) {
  TestHeaderMapImpl headers{{":method", "GET"},
                            {":authority", "www.solo.io"},
                            {":path", "/getsomething"}};
  filter_->decodeHeaders(headers, true);

  EXPECT_FALSE(functionDecodeHeadersCalled_);
}

TEST_F(WrapperFilterTest, CallsChildIfEnabled) {
  initchildroutemeta();
  TestHeaderMapImpl headers{{":method", "GET"},
                            {":authority", "www.solo.io"},
                            {":path", "/getsomething"}};
  filter_->decodeHeaders(headers, true);

  EXPECT_TRUE(functionDecodeHeadersCalled_);
}

} // namespace Http
} // namespace Envoy
