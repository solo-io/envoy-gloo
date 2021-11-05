#include <iostream>

#include "source/common/http/solo_filter_utility.h"

#include "test/mocks/router/mocks.h"

#include "fmt/format.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::_;
using testing::AtLeast;
using testing::Invoke;
using testing::Return;
using testing::ReturnPointee;
using testing::ReturnRef;
using testing::SaveArg;
using testing::WithArg;

namespace Envoy {
namespace Http {

const std::string FILTER_NAME = "filter_name";

class PerFilterConfigUtilTest : public testing::Test {
public:
  std::shared_ptr<Router::MockRoute> route_;

  void SetUp() override {
    route_.reset(new testing::NiceMock<Router::MockRoute>());
  }
};

class RouteSpecificFilterConfig : public Router::RouteSpecificFilterConfig {};

TEST_F(PerFilterConfigUtilTest, NoRouteEntry) {

  RouteSpecificFilterConfig r;

  EXPECT_CALL(*route_, routeEntry()).WillOnce(Return(nullptr));
  EXPECT_CALL(*route_, mostSpecificPerFilterConfig(FILTER_NAME)).WillOnce(Return(&r));

  EXPECT_EQ(
      &r, SoloFilterUtility::resolvePerFilterConfig<RouteSpecificFilterConfig>(
              FILTER_NAME, route_));
}

TEST_F(PerFilterConfigUtilTest, ConfigInRouteEntry) {

  RouteSpecificFilterConfig r;

  EXPECT_CALL(route_->route_entry_, mostSpecificPerFilterConfig(FILTER_NAME))
      .WillOnce(Return(&r));

  EXPECT_CALL(*route_, mostSpecificPerFilterConfig(FILTER_NAME)).Times(0);
  EXPECT_CALL(route_->route_entry_.virtual_host_, mostSpecificPerFilterConfig(FILTER_NAME))
      .Times(0);

  EXPECT_EQ(
      &r, SoloFilterUtility::resolvePerFilterConfig<RouteSpecificFilterConfig>(
              FILTER_NAME, route_));
}

TEST_F(PerFilterConfigUtilTest, CfgInRouteNotInRouteEntry) {

  RouteSpecificFilterConfig r;

  EXPECT_CALL(route_->route_entry_, mostSpecificPerFilterConfig(FILTER_NAME))
      .WillOnce(Return(nullptr));
  EXPECT_CALL(*route_, mostSpecificPerFilterConfig(FILTER_NAME)).WillOnce(Return(&r));
  EXPECT_CALL(route_->route_entry_.virtual_host_, mostSpecificPerFilterConfig(FILTER_NAME))
      .Times(0);

  EXPECT_EQ(
      &r, SoloFilterUtility::resolvePerFilterConfig<RouteSpecificFilterConfig>(
              FILTER_NAME, route_));
}

TEST_F(PerFilterConfigUtilTest, HaveVirtualHost) {

  RouteSpecificFilterConfig r;

  EXPECT_CALL(route_->route_entry_, mostSpecificPerFilterConfig(FILTER_NAME))
      .WillOnce(Return(nullptr));
  EXPECT_CALL(*route_, mostSpecificPerFilterConfig(FILTER_NAME)).WillOnce(Return(nullptr));
  EXPECT_CALL(route_->route_entry_.virtual_host_, mostSpecificPerFilterConfig(FILTER_NAME))
      .WillOnce(Return(&r));

  EXPECT_EQ(
      &r, SoloFilterUtility::resolvePerFilterConfig<RouteSpecificFilterConfig>(
              FILTER_NAME, route_));
}

TEST_F(PerFilterConfigUtilTest, NothingIsStored) {

  EXPECT_EQ(
      nullptr,
      SoloFilterUtility::resolvePerFilterConfig<RouteSpecificFilterConfig>(
          FILTER_NAME, route_));
}

TEST_F(PerFilterConfigUtilTest, NullReturnsNull) {

  EXPECT_EQ(
      nullptr,
      SoloFilterUtility::resolvePerFilterConfig<RouteSpecificFilterConfig>(
          FILTER_NAME, nullptr));
}

} // namespace Http
} // namespace Envoy
