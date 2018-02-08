#include <iostream>

#include "common/http/functional_stream_decoder_base.h"

#include "common/protobuf/utility.h"

#include "test/test_common/utility.h"

#include "common/router/config_impl.h"

#include "fmt/format.h"

#include "test/mocks/upstream/mocks.h"
#include "test/mocks/server/mocks.h"
#include "test/mocks/common.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::Invoke;
using testing::Return;
using testing::ReturnPointee;
using testing::ReturnRef;
using testing::SaveArg;
using testing::WithArg;
using testing::_;

namespace Envoy {
namespace Http {

class FunctionalFilterTester : public FunctionalFilterBase {
  public:
  FunctionalFilterTester(FactoryContext& ctx, const std::string& childname) : FunctionalFilterBase(ctx, childname),
functionDecodeHeadersCalled_(false),
functionDecodeDataCalled_(false),
functionDecodeTrailersCalled_(false){}

  const ProtobufWkt::Struct& getSpec() {return getFunctionSpec();}


   virtual void onDestroy() override{}
virtual FilterHeadersStatus functionDecodeHeaders(HeaderMap &, bool) override {
functionDecodeHeadersCalled_ = true;
    return FilterHeadersStatus::Continue;
}
virtual FilterDataStatus functionDecodeData(Buffer::Instance &, bool) override {
functionDecodeDataCalled_ = true;
    return FilterDataStatus::Continue;

}
virtual FilterTrailersStatus functionDecodeTrailers(HeaderMap &) override {
functionDecodeTrailersCalled_ = true;
    return FilterTrailersStatus::Continue;

}
bool functionDecodeHeadersCalled_;
bool functionDecodeDataCalled_;
bool functionDecodeTrailersCalled_;
};


class FuncitonFilterTest : public testing::Test {
public:
  FuncitonFilterTest() :childname_("childfilter"){}

protected:
  void SetUp() override  {
    
    initFilter();

    Upstream::MockClusterInfo& info = *factory_context_.cluster_manager_.thread_local_cluster_.cluster_.info_;
    ON_CALL(info, metadata())
    .WillByDefault(ReturnRef(cluster_metadata_));

    Router::MockRouteEntry& routerentry = filter_callbacks_.route_->route_entry_;
    ON_CALL(routerentry, metadata())
      .WillByDefault(ReturnRef(route_metadata_));

  }

  void initFilter() {
    filter_ = std::make_shared<FunctionalFilterTester>(factory_context_, childname_);
    filter_->setDecoderFilterCallbacks(filter_callbacks_);
  }

  void initclustermeta() {

  //TODO use const
    ProtobufWkt::Struct& functionsstruct = (*cluster_metadata_.mutable_filter_metadata())["io.solo.function_router"];
    ProtobufWkt::Value& functionstructvalue = (*functionsstruct.mutable_fields())["functions"];
    ProtobufWkt::Struct* functionstruct = functionstructvalue.mutable_struct_value();
    ProtobufWkt::Value& functionstructspecvalue = (*functionstruct->mutable_fields())["funcname"];

    functionsspecstruct_ = functionstructspecvalue.mutable_struct_value();

    // mark the cluster as functional (i.e. the function filter has claims on it.)
    (*cluster_metadata_.mutable_filter_metadata())[childname_] = ProtobufWkt::Struct();
  }

  void initroutemeta() {

    ProtobufWkt::Value functionvalue;
    functionvalue.set_string_value("funcname");

    ProtobufWkt::Struct routefunctionmeta;
    (*routefunctionmeta.mutable_fields())["function"]  = functionvalue;

  //TODO use const
    (*route_metadata_.mutable_filter_metadata())["io.solo.function_router"] = routefunctionmeta;
    

  }

  NiceMock<Envoy::Http::MockStreamDecoderFilterCallbacks> filter_callbacks_;
  NiceMock<Envoy::Server::Configuration::MockFactoryContext> factory_context_;
  NiceMock<Envoy::Event::MockTimer>* attachmentTimeout_timer_{};
  std::shared_ptr<FunctionalFilterTester> filter_;
  std::deque<Envoy::Http::AsyncClient::Callbacks*> callbacks_;

  ProtobufWkt::Struct* functionsspecstruct_;

  envoy::api::v2::Metadata route_metadata_; 
  envoy::api::v2::Metadata cluster_metadata_;

  std::string childname_;
};

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
  EXPECT_CALL(factory_context_.cluster_manager_, get(clustername));

    Envoy::Http::TestHeaderMapImpl headers{{":method", "GET"},
                                           {":authority", "www.solo.io"},
                                           {":path", "/getsomething"}};
  filter_->decodeHeaders(headers, true);

  
  const ProtobufWkt::Struct& receivedspec = filter_->getSpec();
  const ProtobufWkt::Struct* receivedspecptr = &receivedspec;
  // compare pointers - to make sure no copy happened along the way.
  const ProtobufWkt::Struct* origspecptr = functionsspecstruct_;
  EXPECT_EQ(origspecptr, receivedspecptr);

  EXPECT_TRUE(filter_->functionDecodeHeadersCalled_);
  EXPECT_FALSE(filter_->functionDecodeDataCalled_);
  EXPECT_FALSE(filter_->functionDecodeTrailersCalled_);
}

TEST_F(FuncitonFilterTest, MissingRouteMeta) {
  initclustermeta();

  std::string status;

  EXPECT_CALL(filter_callbacks_, encodeHeaders_(_,_)).WillOnce(Invoke([&](HeaderMap& headers, bool) {
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

}
} // namespace Envoy