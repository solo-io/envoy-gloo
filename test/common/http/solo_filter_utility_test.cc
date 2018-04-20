#include <iostream>

#include "common/http/solo_filter_utility.h"

#include "test/mocks/http/mocks.h"

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

const std::string FILTER_NAME = "filter_name";

class PerFilterConfigUtilTest : public testing::Test {
public:
  NiceMock<MockStreamDecoderFilterCallbacks> filter_callbacks_;
};

class RouteSpecificFilterConfig : public Router::RouteSpecificFilterConfig {};

TEST_F(PerFilterConfigUtilTest, NoRouteEntry) {

  RouteSpecificFilterConfig r;

  EXPECT_CALL(*filter_callbacks_.route_, routeEntry())
      .WillOnce(Return(nullptr));
  EXPECT_CALL(*filter_callbacks_.route_, perFilterConfig(FILTER_NAME))
      .WillOnce(Return(&r));

  PerFilterConfigUtil<RouteSpecificFilterConfig> util(FILTER_NAME);
  EXPECT_EQ(&r, util.getPerFilterConfig(filter_callbacks_));
}

TEST_F(PerFilterConfigUtilTest, HaveRouteEntry) {

  RouteSpecificFilterConfig r;

  EXPECT_CALL(filter_callbacks_.route_->route_entry_,
              perFilterConfig(FILTER_NAME))
      .WillOnce(Return(&r));
  EXPECT_CALL(*filter_callbacks_.route_, perFilterConfig(FILTER_NAME)).Times(0);

  PerFilterConfigUtil<RouteSpecificFilterConfig> util(FILTER_NAME);
  EXPECT_EQ(&r, util.getPerFilterConfig(filter_callbacks_));
}

TEST_F(PerFilterConfigUtilTest, HaveRouteEntryNoCfg) {

  RouteSpecificFilterConfig r;

  EXPECT_CALL(filter_callbacks_.route_->route_entry_,
              perFilterConfig(FILTER_NAME))
      .WillOnce(Return(nullptr));
  EXPECT_CALL(*filter_callbacks_.route_, perFilterConfig(FILTER_NAME))
      .WillOnce(Return(&r));

  PerFilterConfigUtil<RouteSpecificFilterConfig> util(FILTER_NAME);
  EXPECT_EQ(&r, util.getPerFilterConfig(filter_callbacks_));
}

TEST_F(PerFilterConfigUtilTest, HaveVirtualHost) {

  RouteSpecificFilterConfig r;

  EXPECT_CALL(filter_callbacks_.route_->route_entry_,
              perFilterConfig(FILTER_NAME))
      .WillOnce(Return(nullptr));
  EXPECT_CALL(*filter_callbacks_.route_, perFilterConfig(FILTER_NAME))
      .WillOnce(Return(nullptr));
  EXPECT_CALL(filter_callbacks_.route_->route_entry_.virtual_host_,
              perFilterConfig(FILTER_NAME))
      .WillOnce(Return(&r));

  PerFilterConfigUtil<RouteSpecificFilterConfig> util(FILTER_NAME);
  EXPECT_EQ(&r, util.getPerFilterConfig(filter_callbacks_));
}

TEST_F(PerFilterConfigUtilTest, NothingIsStored) {

  PerFilterConfigUtil<RouteSpecificFilterConfig> util(FILTER_NAME);
  EXPECT_EQ(nullptr, util.getPerFilterConfig(filter_callbacks_));
}

TEST_F(PerFilterConfigUtilTest, RouteInfoIsStored) {

  RouteSpecificFilterConfig r;
  EXPECT_CALL(*filter_callbacks_.route_, perFilterConfig(FILTER_NAME))
      .WillOnce(Return(&r));
  long use_count = filter_callbacks_.route_.use_count();

  {
    PerFilterConfigUtil<RouteSpecificFilterConfig> util(FILTER_NAME);
    util.getPerFilterConfig(filter_callbacks_);
    EXPECT_EQ(use_count + 1, filter_callbacks_.route_.use_count());
  }

  EXPECT_EQ(use_count, filter_callbacks_.route_.use_count());
}

} // namespace Http
} // namespace Envoy
