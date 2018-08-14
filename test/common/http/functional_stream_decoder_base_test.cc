#include <iostream>

#include "envoy/http/metadata_accessor.h"

#include "common/config/solo_well_known_names.h"
#include "common/http/functional_stream_decoder_base.h"
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

class FunctionFilterTest;

class FunctionalFilterTester : public StreamDecoderFilter,
                               public FunctionalFilter {
public:
  // Http::StreamFilterBase
  void onDestroy() override {}

  // Http::StreamDecoderFilter
  FilterHeadersStatus decodeHeaders(HeaderMap &, bool) override;
  FilterDataStatus decodeData(Buffer::Instance &, bool) override;
  FilterTrailersStatus decodeTrailers(HeaderMap &) override;
  void setDecoderFilterCallbacks(StreamDecoderFilterCallbacks &) override {}

  // Http::FunctionalFilter
  bool retrieveFunction(const MetadataAccessor &meta_accessor) override;

  bool decodeHeadersCalled_{false};
  bool decodeDataCalled_{false};
  bool decodeTrailersCalled_{false};
  std::string functionCalled_;
  bool routeMetadataFound_;

  const MetadataAccessor *meta_accessor_;
};

typedef FunctionalFilterMixin<FunctionalFilterTester>
    MixedFunctionalFilterTester;

class FunctionFilterTest : public testing::Test {
public:
  FunctionFilterTest() : childname_("childfilter") {}

protected:
  void SetUp() override {

    initFilter();

    Upstream::MockClusterInfo &info =
        *factory_context_.cluster_manager_.thread_local_cluster_.cluster_.info_;
    ON_CALL(info, metadata()).WillByDefault(ReturnRef(cluster_metadata_));

    Router::MockRouteEntry &routerentry =
        filter_callbacks_.route_->route_entry_;
    ON_CALL(routerentry, metadata()).WillByDefault(ReturnRef(route_metadata_));
  }

  void initFilter() {
    filter_ = std::make_unique<FunctionalFilterMixin<FunctionalFilterTester>>(
        factory_context_, childname_);
    filter_->setDecoderFilterCallbacks(filter_callbacks_);
  }

  void initClusterMeta(bool passthrough = false) {

    // TODO use const
    ProtobufWkt::Struct &functionsstruct =
        (*cluster_metadata_.mutable_filter_metadata())
            [Config::SoloCommonMetadataFilters::get().FUNCTIONAL_ROUTER];

    // passthrough to false for tests
    (*functionsstruct.mutable_fields())
        [Config::MetadataFunctionalRouterKeys::get().PASSTHROUGH]
            .set_bool_value(passthrough);

    ProtobufWkt::Value &functionstructvalue =
        (*functionsstruct.mutable_fields())
            [Config::MetadataFunctionalRouterKeys::get().FUNCTIONS];
    ProtobufWkt::Struct *functionstruct =
        functionstructvalue.mutable_struct_value();
    ProtobufWkt::Value &functionstructspecvalue =
        (*functionstruct->mutable_fields())[functionname_];

    cluster_meta_function_spec_struct_ =
        functionstructspecvalue.mutable_struct_value();
    (*cluster_meta_function_spec_struct_->mutable_fields())["name"]
        .set_string_value(functionname_);

    ProtobufWkt::Value &function2structspecvalue =
        (*functionstruct->mutable_fields())[function2name_];

    cluster_meta_function2_spec_struct_ =
        function2structspecvalue.mutable_struct_value();
    (*cluster_meta_function2_spec_struct_->mutable_fields())["name"]
        .set_string_value(function2name_);

    // mark the cluster as functional (i.e. the function filter has claims on
    // it.)
    (*cluster_metadata_.mutable_filter_metadata())[childname_] =
        ProtobufWkt::Struct();
    cluster_meta_child_spec_struct_ =
        &((*cluster_metadata_.mutable_filter_metadata())[childname_]);
  }

  void initchildroutemeta() {

    ProtobufWkt::Struct routefunctionmeta;
    (*route_metadata_.mutable_filter_metadata())[childname_] =
        routefunctionmeta;
    route_meta_child_spec_struct_ =
        &((*route_metadata_.mutable_filter_metadata())[childname_]);
  }

  void initRoutePerFilter() {
    route_function_.function_name_ = functionname_;

    ON_CALL(
        filter_callbacks_.route_->route_entry_,
        perFilterConfig(Config::SoloCommonFilterNames::get().FUNCTIONAL_ROUTER))
        .WillByDefault(Return(&route_function_));
  }

  void initRouteMeta() {

    ProtobufWkt::Value functionvalue;
    functionvalue.set_string_value(functionname_);

    ProtobufWkt::Value clustervalue;
    ProtobufWkt::Struct *clusterstruct = clustervalue.mutable_struct_value();
    (*clusterstruct->mutable_fields())
        [Config::MetadataFunctionalRouterKeys::get().FUNCTION] = functionvalue;

    auto clustername = filter_callbacks_.route_->route_entry_.cluster_name_;

    ProtobufWkt::Struct routefunctionmeta;
    (*routefunctionmeta.mutable_fields())[clustername] = clustervalue;

    // TODO use const
    (*route_metadata_.mutable_filter_metadata())
        [Config::SoloCommonMetadataFilters::get().FUNCTIONAL_ROUTER] =
            routefunctionmeta;

    (*route_metadata_.mutable_filter_metadata())
        [Config::SoloCommonMetadataFilters::get().FUNCTIONAL_ROUTER] =
            routefunctionmeta;
  }

  void initRouteMetamultiple() {

    ProtobufWkt::Struct multifunction;
    auto clustername = filter_callbacks_.route_->route_entry_.cluster_name_;
    // TODO(yuval-k): constify + use class vars for functions
    std::string json = "{ \"" + clustername + "\":{" + R"EOF(
        "weighted_functions": {
            "total_weight": 10,
            "functions": [
                {"name":"funcname","weight":5},
                {"name":"funcname2","weight":5}
            ]
        }
        }
        }
        )EOF";
    MessageUtil::loadFromJson(json, multifunction);

    (*route_metadata_.mutable_filter_metadata())
        [Config::SoloCommonMetadataFilters::get().FUNCTIONAL_ROUTER] =
            multifunction;
  }

  NiceMock<MockStreamDecoderFilterCallbacks> filter_callbacks_;
  NiceMock<Server::Configuration::MockFactoryContext> factory_context_;
  NiceMock<Event::MockTimer> *attachmentTimeout_timer_{};
  std::unique_ptr<MixedFunctionalFilterTester> filter_;

  ProtobufWkt::Struct *route_meta_child_spec_struct_;
  ProtobufWkt::Struct *cluster_meta_child_spec_struct_;

  envoy::api::v2::core::Metadata route_metadata_;

  ProtobufWkt::Struct *cluster_meta_function_spec_struct_;
  ProtobufWkt::Struct *cluster_meta_function2_spec_struct_;
  envoy::api::v2::core::Metadata cluster_metadata_;

  std::string childname_;
  std::string functionname_{"funcname"};
  std::string function2name_{"funcname2"};

  Http::FunctionalFilterMixinRouteFilterConfig route_function_;
};

bool FunctionalFilterTester::retrieveFunction(
    const MetadataAccessor &meta_accessor) {
  bool have_function = meta_accessor.getFunctionSpec().has_value();
  meta_accessor_ = &meta_accessor;
  return have_function;
}

FilterHeadersStatus FunctionalFilterTester::decodeHeaders(HeaderMap &, bool) {
  routeMetadataFound_ = meta_accessor_->getRouteMetadata().has_value();
  decodeHeadersCalled_ = true;
  functionCalled_ = (*meta_accessor_->getFunctionSpec().value())
                        .fields()
                        .at("name")
                        .string_value();
  return FilterHeadersStatus::Continue;
}

FilterDataStatus FunctionalFilterTester::decodeData(Buffer::Instance &, bool) {
  decodeDataCalled_ = true;
  return FilterDataStatus::Continue;
}

FilterTrailersStatus FunctionalFilterTester::decodeTrailers(HeaderMap &) {
  decodeTrailersCalled_ = true;
  return FilterTrailersStatus::Continue;
}

TEST_F(FunctionFilterTest, NothingConfigured) {

  TestHeaderMapImpl headers{{":method", "GET"},
                            {":authority", "www.solo.io"},
                            {":path", "/getsomething"}};
  filter_->decodeHeaders(headers, true);

  EXPECT_FALSE(filter_->decodeHeadersCalled_);
  EXPECT_FALSE(filter_->decodeDataCalled_);
  EXPECT_FALSE(filter_->decodeTrailersCalled_);
}

TEST_F(FunctionFilterTest, HappyPathPerFilter) {
  initClusterMeta();
  initRoutePerFilter();

  TestHeaderMapImpl headers{{":method", "GET"},
                            {":authority", "www.solo.io"},
                            {":path", "/getsomething"}};
  filter_->decodeHeaders(headers, true);

  EXPECT_TRUE(filter_->decodeHeadersCalled_);
  EXPECT_EQ(functionname_, filter_->functionCalled_);
}

TEST_F(FunctionFilterTest, ClusterNotFoundIsIgnored) {
  initClusterMeta();
  initRouteMeta();
  auto clustername = filter_callbacks_.route_->route_entry_.cluster_name_;
  EXPECT_CALL(factory_context_.cluster_manager_, get(clustername))
      .WillRepeatedly(Return(nullptr));

  TestHeaderMapImpl headers{{":method", "GET"},
                            {":authority", "www.solo.io"},
                            {":path", "/getsomething"}};
  filter_->decodeHeaders(headers, true);

  EXPECT_EQ(nullptr, filter_->meta_accessor_);

  EXPECT_FALSE(filter_->decodeHeadersCalled_);
  EXPECT_FALSE(filter_->decodeDataCalled_);
  EXPECT_FALSE(filter_->decodeTrailersCalled_);
}

TEST_F(FunctionFilterTest, HaveRouteMeta) {
  initClusterMeta();
  initRouteMeta();
  auto clustername = filter_callbacks_.route_->route_entry_.cluster_name_;
  EXPECT_CALL(factory_context_.cluster_manager_, get(clustername))
      .Times(AtLeast(1));

  TestHeaderMapImpl headers{{":method", "GET"},
                            {":authority", "www.solo.io"},
                            {":path", "/getsomething"}};
  filter_->decodeHeaders(headers, true);

  const ProtobufWkt::Struct &receivedspec =
      *filter_->meta_accessor_->getFunctionSpec().value();

  EXPECT_NE(nullptr, &receivedspec);

  EXPECT_TRUE(filter_->decodeHeadersCalled_);
  EXPECT_FALSE(filter_->decodeDataCalled_);
  EXPECT_FALSE(filter_->decodeTrailersCalled_);
}

TEST_F(FunctionFilterTest, MetaNoCopy) {
  initClusterMeta();
  initRouteMeta();
  initchildroutemeta();

  TestHeaderMapImpl headers{{":method", "GET"},
                            {":authority", "www.solo.io"},
                            {":path", "/getsomething"}};
  filter_->decodeHeaders(headers, true);

  const ProtobufWkt::Struct *funcspec =
      filter_->meta_accessor_->getFunctionSpec().value();
  const ProtobufWkt::Struct *childspec =
      filter_->meta_accessor_->getClusterMetadata().value();
  const ProtobufWkt::Struct *routechildspec =
      filter_->meta_accessor_->getRouteMetadata().value();

  EXPECT_NE(nullptr, routechildspec);

  EXPECT_EQ(cluster_meta_function_spec_struct_, funcspec);
  EXPECT_EQ(cluster_meta_child_spec_struct_, childspec);
  EXPECT_EQ(route_meta_child_spec_struct_, routechildspec);
}

TEST_F(FunctionFilterTest, MissingRouteMetaPassThrough) {
  initClusterMeta(true);

  TestHeaderMapImpl headers{{":method", "GET"},
                            {":authority", "www.solo.io"},
                            {":path", "/getsomething"}};
  FilterHeadersStatus headerstatus = filter_->decodeHeaders(headers, true);
  EXPECT_FALSE(filter_->decodeHeadersCalled_);
  EXPECT_EQ(FilterHeadersStatus::Continue, headerstatus);
}

TEST_F(FunctionFilterTest, MissingRouteMeta) {
  initClusterMeta();

  std::string status;

  EXPECT_CALL(filter_callbacks_, encodeHeaders_(_, _))
      .WillOnce(Invoke([&](HeaderMap &headers, bool) {
        status = headers.Status()->value().c_str();
      }));

  TestHeaderMapImpl headers{{":method", "GET"},
                            {":authority", "www.solo.io"},
                            {":path", "/getsomething"}};
  filter_->decodeHeaders(headers, true);

  // test that we needed errored
  EXPECT_EQ(status, "404");

  EXPECT_FALSE(filter_->decodeHeadersCalled_);
  EXPECT_FALSE(filter_->decodeDataCalled_);
  EXPECT_FALSE(filter_->decodeTrailersCalled_);
}

TEST_F(FunctionFilterTest, MissingChildRouteMeta) {
  initClusterMeta();
  initRouteMeta();

  TestHeaderMapImpl headers{{":method", "GET"},
                            {":authority", "www.solo.io"},
                            {":path", "/getsomething"}};
  filter_->decodeHeaders(headers, true);

  EXPECT_FALSE(filter_->routeMetadataFound_);
}

TEST_F(FunctionFilterTest, FoundChildRouteMeta) {
  initClusterMeta();
  initRouteMeta();
  initchildroutemeta();

  TestHeaderMapImpl headers{{":method", "GET"},
                            {":authority", "www.solo.io"},
                            {":path", "/getsomething"}};
  filter_->decodeHeaders(headers, true);

  EXPECT_TRUE(filter_->routeMetadataFound_);
}

TEST_F(FunctionFilterTest, FindMultiFunctions) {
  initClusterMeta();
  initRouteMetamultiple();
  EXPECT_CALL(factory_context_.random_, random()).WillOnce(Return(0));

  TestHeaderMapImpl headers{{":method", "GET"},
                            {":authority", "www.solo.io"},
                            {":path", "/getsomething"}};
  filter_->decodeHeaders(headers, true);

  EXPECT_TRUE(filter_->decodeHeadersCalled_);
  EXPECT_EQ(functionname_, filter_->functionCalled_);
}

TEST_F(FunctionFilterTest, FindMultiFunctions2) {
  initClusterMeta();
  initRouteMetamultiple();

  EXPECT_CALL(factory_context_.random_, random()).WillOnce(Return(6));

  TestHeaderMapImpl headers{{":method", "GET"},
                            {":authority", "www.solo.io"},
                            {":path", "/getsomething"}};
  filter_->decodeHeaders(headers, true);

  EXPECT_TRUE(filter_->decodeHeadersCalled_);
  EXPECT_EQ(function2name_, filter_->functionCalled_);
}

} // namespace Http
} // namespace Envoy
