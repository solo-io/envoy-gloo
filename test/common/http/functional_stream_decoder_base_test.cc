#include <iostream>

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

class FuncitonFilterTest;

class FunctionalFilterTester : public StreamDecoderFilter,
                               public FunctionRetriever {
public:
  virtual FilterHeadersStatus decodeHeaders(HeaderMap &, bool) override;
  virtual FilterDataStatus decodeData(Buffer::Instance &, bool) override;
  virtual FilterTrailersStatus decodeTrailers(HeaderMap &) override;
  void onDestroy() override {}
  void setDecoderFilterCallbacks(StreamDecoderFilterCallbacks &) override {}

  virtual bool retrieveFunction(const MetadataAccessor &meta_accessor) override;

  bool functionDecodeHeadersCalled_{false};
  bool functionDecodeDataCalled_{false};
  bool functionDecodeTrailersCalled_{false};
  std::string functionCalled_;
  bool routeMetadataFound_;

  const MetadataAccessor *meta_accessor_;
};

class FuncitonFilterTest : public testing::Test {
public:
  FuncitonFilterTest() : childname_("childfilter") {}

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

  void initclustermeta() {

    // TODO use const
    ProtobufWkt::Struct &functionsstruct =
        (*cluster_metadata_.mutable_filter_metadata())
            [Config::SoloCommonMetadataFilters::get().FUNCTIONAL_ROUTER];
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

  void initroutemeta() {

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

  void initroutemetamultiple() {

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

  NiceMock<Envoy::Http::MockStreamDecoderFilterCallbacks> filter_callbacks_;
  NiceMock<Envoy::Server::Configuration::MockFactoryContext> factory_context_;
  NiceMock<Envoy::Event::MockTimer> *attachmentTimeout_timer_{};
  std::unique_ptr<FunctionalFilterTester> filter_;

  ProtobufWkt::Struct *route_meta_child_spec_struct_;
  ProtobufWkt::Struct *cluster_meta_child_spec_struct_;

  envoy::api::v2::core::Metadata route_metadata_;

  ProtobufWkt::Struct *cluster_meta_function_spec_struct_;
  ProtobufWkt::Struct *cluster_meta_function2_spec_struct_;
  envoy::api::v2::core::Metadata cluster_metadata_;

  std::string childname_;
  std::string functionname_{"funcname"};
  std::string function2name_{"funcname2"};
};

bool FunctionalFilterTester::retrieveFunction(
    const MetadataAccessor &meta_accessor) {
  bool have_function = meta_accessor.getFunctionSpec().valid();
  meta_accessor_ = &meta_accessor;
  return have_function;
}

FilterHeadersStatus FunctionalFilterTester::decodeHeaders(HeaderMap &, bool) {
  routeMetadataFound_ = meta_accessor_->getRouteMetadata().valid();
  functionDecodeHeadersCalled_ = true;
  functionCalled_ = (*meta_accessor_->getFunctionSpec().value())
                        .fields()
                        .at("name")
                        .string_value();
  return FilterHeadersStatus::Continue;
}

FilterDataStatus FunctionalFilterTester::decodeData(Buffer::Instance &, bool) {
  functionDecodeDataCalled_ = true;
  return FilterDataStatus::Continue;
}

FilterTrailersStatus FunctionalFilterTester::decodeTrailers(HeaderMap &) {
  functionDecodeTrailersCalled_ = true;
  return FilterTrailersStatus::Continue;
}

TEST_F(FuncitonFilterTest, NothingConfigured) {

  Envoy::Http::TestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"}};
  filter_->decodeHeaders(headers, true);

  EXPECT_FALSE(filter_->functionDecodeHeadersCalled_);
  EXPECT_FALSE(filter_->functionDecodeDataCalled_);
  EXPECT_FALSE(filter_->functionDecodeTrailersCalled_);
}

TEST_F(FuncitonFilterTest, HaveRouteMeta) {
  initclustermeta();
  initroutemeta();
  auto clustername = filter_callbacks_.route_->route_entry_.cluster_name_;
  EXPECT_CALL(factory_context_.cluster_manager_, get(clustername))
      .Times(AtLeast(1));

  Envoy::Http::TestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"}};
  filter_->decodeHeaders(headers, true);

  const ProtobufWkt::Struct &receivedspec =
      *filter_->meta_accessor_->getFunctionSpec().value();

  EXPECT_NE(nullptr, &receivedspec);

  EXPECT_TRUE(filter_->functionDecodeHeadersCalled_);
  EXPECT_FALSE(filter_->functionDecodeDataCalled_);
  EXPECT_FALSE(filter_->functionDecodeTrailersCalled_);
}

TEST_F(FuncitonFilterTest, MetaNoCopy) {
  initclustermeta();
  initroutemeta();
  initchildroutemeta();

  Envoy::Http::TestHeaderMapImpl headers{{":method", "GET"},
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

TEST_F(FuncitonFilterTest, MissingRouteMeta) {
  initclustermeta();

  std::string status;

  EXPECT_CALL(filter_callbacks_, encodeHeaders_(_, _))
      .WillOnce(Invoke([&](HeaderMap &headers, bool) {
        status = headers.Status()->value().c_str();
      }));

  Envoy::Http::TestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"}};
  filter_->decodeHeaders(headers, true);

  // test that we needed errored
  EXPECT_EQ(status, "404");

  EXPECT_FALSE(filter_->functionDecodeHeadersCalled_);
  EXPECT_FALSE(filter_->functionDecodeDataCalled_);
  EXPECT_FALSE(filter_->functionDecodeTrailersCalled_);
}

TEST_F(FuncitonFilterTest, MissingChildRouteMeta) {
  initclustermeta();
  initroutemeta();

  Envoy::Http::TestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"}};
  filter_->decodeHeaders(headers, true);

  EXPECT_FALSE(filter_->routeMetadataFound_);
}

TEST_F(FuncitonFilterTest, FoundChildRouteMeta) {
  initclustermeta();
  initroutemeta();
  initchildroutemeta();

  Envoy::Http::TestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"}};
  filter_->decodeHeaders(headers, true);

  EXPECT_TRUE(filter_->routeMetadataFound_);
}

TEST_F(FuncitonFilterTest, FindMultiFunctions) {
  initclustermeta();
  initroutemetamultiple();
  EXPECT_CALL(factory_context_.random_, random()).WillOnce(Return(0));

  Envoy::Http::TestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"}};
  filter_->decodeHeaders(headers, true);

  EXPECT_TRUE(filter_->functionDecodeHeadersCalled_);
  EXPECT_EQ(functionname_, filter_->functionCalled_);
}

TEST_F(FuncitonFilterTest, FindMultiFunctions2) {
  initclustermeta();
  initroutemetamultiple();

  EXPECT_CALL(factory_context_.random_, random()).WillOnce(Return(6));

  Envoy::Http::TestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/getsomething"}};
  filter_->decodeHeaders(headers, true);

  EXPECT_TRUE(filter_->functionDecodeHeadersCalled_);
  EXPECT_EQ(function2name_, filter_->functionCalled_);
}

} // namespace Http
} // namespace Envoy
