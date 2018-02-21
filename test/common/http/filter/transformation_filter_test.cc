#include "common/http/filter/transformation_filter.h"

#include "server/config/http/transformation_filter_config_factory.h"

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

using Http::TransformationFilterConfig;
using Server::Configuration::TransformationFilterConfigFactory;


TEST(TransformationFilterConfigFactory, EmptyConfig) {
  envoy::api::v2::filter::http::Transformations config;

  // shouldnt throw.
  TransformationFilterConfig cfg(config);
}

} // namespace Http
} // namespace Envoy
