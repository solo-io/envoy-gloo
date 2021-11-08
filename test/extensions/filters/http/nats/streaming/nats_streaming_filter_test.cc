#include "source/extensions/filters/http/nats/streaming/nats_streaming_filter.h"
#include "source/extensions/filters/http/nats/streaming/nats_streaming_filter_config.h"
#include "source/extensions/filters/http/nats/streaming/nats_streaming_filter_config_factory.h"

#include "test/mocks/http/mocks.h"
#include "test/mocks/nats/streaming/mocks.h"
#include "test/mocks/server/mocks.h"
#include "test/mocks/upstream/mocks.h"

#include "api/envoy/config/filter/http/nats/streaming/v2/nats_streaming.pb.validate.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::_;
using testing::NiceMock;
using testing::Ref;
using testing::Return;

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Nats {
namespace Streaming {

class NatsStreamingFilterTest : public testing::Test {
public:
  NatsStreamingFilterTest() {}

protected:
  void SetUp() override {
    envoy::config::filter::http::nats::streaming::v2::NatsStreaming
        proto_config;
    proto_config.mutable_op_timeout()->set_nanos(17 * 1000000);
    proto_config.set_max_connections(1);
    proto_config.set_cluster("cluster");
    factory_context_.cluster_manager_.initializeClusters({"cluster"}, {});
    factory_context_.cluster_manager_.initializeThreadLocalClusters({"cluster"});

    config_.reset(new NatsStreamingFilterConfig(
        proto_config, factory_context_.clusterManager()));
    nats_streaming_client_.reset(
        new NiceMock<Envoy::Nats::Streaming::MockClient>);
    filter_.reset(new NatsStreamingFilter(config_, nats_streaming_client_));
    filter_->setDecoderFilterCallbacks(callbacks_);
  }

  NatsStreamingRouteSpecificFilterConfig
  routeSpecificFilterConfig(const std::string &subject,
                            const std::string &clusterId,
                            const std::string &discoverPrefix) {
    return NatsStreamingRouteSpecificFilterConfig(
        perRouteProtoConfig(subject, clusterId, discoverPrefix));
  }

  NiceMock<Envoy::Server::Configuration::MockFactoryContext> factory_context_;
  NatsStreamingFilterConfigSharedPtr config_;
  std::shared_ptr<NiceMock<Envoy::Nats::Streaming::MockClient>>
      nats_streaming_client_;
  std::unique_ptr<NatsStreamingFilter> filter_;
  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks_;

private:
  envoy::config::filter::http::nats::streaming::v2::NatsStreamingPerRoute
  perRouteProtoConfig(const std::string &subject, const std::string &clusterId,
                      const std::string &discoverPrefix) {
    envoy::config::filter::http::nats::streaming::v2::NatsStreamingPerRoute
        proto_config;
    proto_config.set_subject(subject);
    proto_config.set_cluster_id(clusterId);
    proto_config.set_discover_prefix(discoverPrefix);
    return proto_config;
  }
};

TEST_F(NatsStreamingFilterTest, NoSubjectHeaderOnlyRequest) {
  // `nats_streaming_client_->makeRequest()` should not be called.
  EXPECT_CALL(*nats_streaming_client_, makeRequest_(_, _, _, _, _)).Times(0);

  Http::TestRequestHeaderMapImpl headers;
  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            filter_->decodeHeaders(headers, true));
}

TEST_F(NatsStreamingFilterTest, NoSubjectRequestWithData) {
  // `nats_streaming_client_->makeRequest()` should not be called.
  EXPECT_CALL(*nats_streaming_client_, makeRequest_(_, _, _, _, _)).Times(0);

  callbacks_.buffer_.reset(new Buffer::OwnedImpl);

  Http::TestRequestHeaderMapImpl headers;
  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            filter_->decodeHeaders(headers, false));

  Buffer::OwnedImpl data1("hello");
  callbacks_.buffer_->add(data1);
  EXPECT_EQ(Http::FilterDataStatus::Continue,
            filter_->decodeData(data1, false));

  Buffer::OwnedImpl data2(" world");
  callbacks_.buffer_->add(data2);
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(data2, true));

  // Assert that no payload has been sent using the NATS Streaming client.
  EXPECT_EQ(0, nats_streaming_client_->last_payload_.length());
}

TEST_F(NatsStreamingFilterTest, NoSubjectRequestWithTrailers) {
  // `nats_streaming_client_->makeRequest()` should not be called.
  EXPECT_CALL(*nats_streaming_client_, makeRequest_(_, _, _, _, _)).Times(0);

  callbacks_.buffer_.reset(new Buffer::OwnedImpl);

  Http::TestRequestHeaderMapImpl headers;
  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            filter_->decodeHeaders(headers, false));

  Buffer::OwnedImpl data1("hello");
  callbacks_.buffer_->add(data1);
  EXPECT_EQ(Http::FilterDataStatus::Continue,
            filter_->decodeData(data1, false));

  Buffer::OwnedImpl data2(" world");
  callbacks_.buffer_->add(data2);
  EXPECT_EQ(Http::FilterDataStatus::Continue,
            filter_->decodeData(data2, false));

  Http::TestRequestTrailerMapImpl trailers;
  EXPECT_EQ(Envoy::Http::FilterTrailersStatus::Continue,
            filter_->decodeTrailers(trailers));

  // Assert that no payload has been sent using the NATS Streaming client.
  EXPECT_EQ(0, nats_streaming_client_->last_payload_.length());
}

TEST_F(NatsStreamingFilterTest, HeaderOnlyRequest) {
  // `nats_streaming_client_->makeRequest()` should be called exactly once.
  EXPECT_CALL(*nats_streaming_client_,
              makeRequest_("Subject1", "cluster_id", "discover_prefix1", _,
                           Ref(*filter_)))
      .Times(1);

  const auto &name = SoloHttpFilterNames::get().NatsStreaming;
  const auto &&config =
      routeSpecificFilterConfig("Subject1", "cluster_id", "discover_prefix1");
  ON_CALL(*callbacks_.route_, mostSpecificPerFilterConfig(name))
      .WillByDefault(Return(&config));

  Http::TestRequestHeaderMapImpl headers;
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->decodeHeaders(headers, true));

  EXPECT_TRUE(nats_streaming_client_->last_payload_.empty());
}

TEST_F(NatsStreamingFilterTest, RequestWithData) {
  // `nats_streaming_client_->makeRequest()` should be called exactly once.
  EXPECT_CALL(*nats_streaming_client_,
              makeRequest_("Subject1", "cluster_id", "discover_prefix1", _,
                           Ref(*filter_)))
      .Times(1);

  const auto &name = SoloHttpFilterNames::get().NatsStreaming;
  const auto &&config =
      routeSpecificFilterConfig("Subject1", "cluster_id", "discover_prefix1");
  ON_CALL(*callbacks_.route_, mostSpecificPerFilterConfig(name))
      .WillByDefault(Return(&config));

  callbacks_.buffer_.reset(new Buffer::OwnedImpl);

  Http::TestRequestHeaderMapImpl headers;
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->decodeHeaders(headers, false));

  Buffer::OwnedImpl data1("hello");
  callbacks_.buffer_->add(data1);
  EXPECT_EQ(Http::FilterDataStatus::StopIterationNoBuffer,
            filter_->decodeData(data1, false));

  Buffer::OwnedImpl data2(" world");
  callbacks_.buffer_->add(data2);
  EXPECT_EQ(Http::FilterDataStatus::StopIterationNoBuffer,
            filter_->decodeData(data2, true));

  pb::Payload actual_payload;
  EXPECT_TRUE(
      actual_payload.ParseFromString(nats_streaming_client_->last_payload_));
  EXPECT_TRUE(actual_payload.headers().empty());
  EXPECT_EQ("hello world", actual_payload.body());
}

TEST_F(NatsStreamingFilterTest, RequestWithHeadersAndOneChunkOfData) {
  // `nats_streaming_client_->makeRequest()` should be called exactly once.
  EXPECT_CALL(*nats_streaming_client_,
              makeRequest_("Subject1", "cluster_id", "discover_prefix1", _,
                           Ref(*filter_)))
      .Times(1);

  const auto &name = SoloHttpFilterNames::get().NatsStreaming;
  const auto &&config =
      routeSpecificFilterConfig("Subject1", "cluster_id", "discover_prefix1");
  ON_CALL(*callbacks_.route_, mostSpecificPerFilterConfig(name))
      .WillByDefault(Return(&config));

  callbacks_.buffer_.reset(new Buffer::OwnedImpl);

  Http::TestRequestHeaderMapImpl headers{{"some-header", "a"},
                                         {"other-header", "b"}};
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->decodeHeaders(headers, false));

  Buffer::OwnedImpl data("hello world");
  callbacks_.buffer_->add(data);
  EXPECT_EQ(Http::FilterDataStatus::StopIterationNoBuffer,
            filter_->decodeData(data, true));

  pb::Payload actual_payload;
  EXPECT_TRUE(
      actual_payload.ParseFromString(nats_streaming_client_->last_payload_));
  EXPECT_EQ("a", actual_payload.headers().at("some-header"));
  EXPECT_EQ("b", actual_payload.headers().at("other-header"));
  EXPECT_EQ("hello world", actual_payload.body());
}

TEST_F(NatsStreamingFilterTest, RequestWithTrailers) {
  // `nats_streaming_client_->makeRequest()` should be called exactly once.
  EXPECT_CALL(*nats_streaming_client_,
              makeRequest_("Subject1", "cluster_id", "discover_prefix1", _,
                           Ref(*filter_)))
      .Times(1);

  const auto &name = SoloHttpFilterNames::get().NatsStreaming;
  const auto &&config =
      routeSpecificFilterConfig("Subject1", "cluster_id", "discover_prefix1");
  ON_CALL(*callbacks_.route_, mostSpecificPerFilterConfig(name))
      .WillByDefault(Return(&config));

  callbacks_.buffer_.reset(new Buffer::OwnedImpl);

  Http::TestRequestHeaderMapImpl headers;
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->decodeHeaders(headers, false));

  Buffer::OwnedImpl data1("hello");
  callbacks_.buffer_->add(data1);
  EXPECT_EQ(Http::FilterDataStatus::StopIterationNoBuffer,
            filter_->decodeData(data1, false));

  Buffer::OwnedImpl data2(" world");
  callbacks_.buffer_->add(data2);
  EXPECT_EQ(Http::FilterDataStatus::StopIterationNoBuffer,
            filter_->decodeData(data2, false));

  Http::TestRequestTrailerMapImpl trailers;
  EXPECT_EQ(Envoy::Http::FilterTrailersStatus::StopIteration,
            filter_->decodeTrailers(trailers));

  pb::Payload actual_payload;
  EXPECT_TRUE(
      actual_payload.ParseFromString(nats_streaming_client_->last_payload_));
  EXPECT_TRUE(actual_payload.headers().empty());
  EXPECT_EQ("hello world", actual_payload.body());
}

} // namespace Streaming
} // namespace Nats
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
