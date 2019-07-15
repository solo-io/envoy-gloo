#include <memory>
#include <string>

#include "envoy/tcp/codec.h"

#include "common/network/utility.h"
#include "common/tcp/conn_pool_impl.h"
#include "common/upstream/upstream_impl.h"

#include "test/mocks/event/mocks.h"
#include "test/mocks/network/mocks.h"
#include "test/mocks/tcp/mocks_nats.h"
#include "test/mocks/thread_local/mocks.h"
#include "test/mocks/upstream/mocks.h"
#include "test/test_common/printers.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::_;
using testing::Eq;
using testing::InSequence;
using testing::Invoke;
using testing::Ref;
using testing::Return;
using testing::ReturnRef;
using testing::SaveArg;

namespace Envoy {
namespace Tcp {
namespace ConnPoolNats {

using T = std::string;
using TPtr = MessagePtr<T>;

bool shouldMetricBeZero(const std::string& name){
  // stats with the word remaining are initialized to not be zero. so we ignore them
  return name.find("remaining") == std::string::npos;
}

class TcpClientImplTest : public testing::Test, public DecoderFactory<T> {
public:
  // Tcp::DecoderFactory
  DecoderPtr create(DecoderCallbacks<T> &callbacks) override {
    callbacks_ = &callbacks;
    return DecoderPtr{decoder_};
  }

  ~TcpClientImplTest() {
    client_.reset();

    // Make sure all gauges are 0.
    for (const Stats::GaugeSharedPtr &gauge :
         host_->cluster_.stats_store_.gauges()) {
      if (shouldMetricBeZero(gauge->name())){
        EXPECT_EQ(0U, gauge->value()) << "cluster." << gauge->name() << " is " << gauge->value();
      }
    }
    for (const Stats::GaugeSharedPtr &gauge : host_->stats_store_.gauges()) {
      if (shouldMetricBeZero(gauge->name())){
        EXPECT_EQ(0U, gauge->value()) << "host." << gauge->name() << " is " << gauge->value();
      }
    }
  }

  void setup() {
    config_.reset(new ConfigImpl());
    finishSetup();
  }

  void setup(std::unique_ptr<Config> &&config) {
    config_ = std::move(config);
    finishSetup();
  }

  void finishSetup() {
    upstream_connection_ = new NiceMock<Network::MockClientConnection>();
    Upstream::MockHost::MockCreateConnectionData conn_info;
    conn_info.connection_ = upstream_connection_;
    EXPECT_CALL(*host_, createConnection_(_, _)).WillOnce(Return(conn_info));
    EXPECT_CALL(*upstream_connection_, addReadFilter(_))
        .WillOnce(SaveArg<0>(&upstream_read_filter_));
    EXPECT_CALL(*upstream_connection_, connect());
    EXPECT_CALL(*upstream_connection_, noDelay(true));

    client_ = ClientImpl<T>::create(host_, dispatcher_, EncoderPtr<T>{encoder_},
                                    *this, pool_callbacks_, *config_);
    EXPECT_EQ(1UL, host_->cluster_.stats_.upstream_cx_total_.value());
    EXPECT_EQ(1UL, host_->stats_.cx_total_.value());
  }

  void onConnected() {
    upstream_connection_->raiseEvent(Network::ConnectionEvent::Connected);
  }

  const std::string cluster_name_{"foo"};
  std::shared_ptr<Upstream::MockHost> host_{new NiceMock<Upstream::MockHost>()};
  Event::MockDispatcher dispatcher_;
  MockEncoder *encoder_{new MockEncoder()};
  MockDecoder *decoder_{new MockDecoder()};
  DecoderCallbacks<T> *callbacks_{};
  MockPoolCallbacks pool_callbacks_;
  NiceMock<Network::MockClientConnection> *upstream_connection_{};
  Network::ReadFilterSharedPtr upstream_read_filter_;
  std::unique_ptr<Config> config_;
  ClientPtr<T> client_;
};

TEST_F(TcpClientImplTest, Basic) {
  InSequence s;

  setup();

  T request1;
  EXPECT_CALL(*encoder_, encode(Ref(request1), _));
  client_->makeRequest(request1);

  onConnected();

  T request2;
  EXPECT_CALL(*encoder_, encode(Ref(request2), _));
  client_->makeRequest(request2);

  EXPECT_EQ(2UL, host_->cluster_.stats_.upstream_rq_total_.value());
  EXPECT_EQ(2UL, host_->stats_.rq_total_.value());

  // TODO(talnordan): What should be counted as an active request?
  EXPECT_EQ(0UL, host_->cluster_.stats_.upstream_rq_active_.value());
  EXPECT_EQ(0UL, host_->stats_.rq_active_.value());

  Buffer::OwnedImpl fake_data;
  EXPECT_CALL(*decoder_, decode(Ref(fake_data)))
      .WillOnce(Invoke([&](Buffer::Instance &) -> void {
        InSequence s;
        TPtr response1(new T());
        EXPECT_CALL(pool_callbacks_, onResponse_(Ref(response1)));
        EXPECT_CALL(host_->outlier_detector_,
                    putResult(Upstream::Outlier::Result::EXT_ORIGIN_REQUEST_SUCCESS, _));
        callbacks_->onValue(std::move(response1));

        TPtr response2(new T());
        EXPECT_CALL(pool_callbacks_, onResponse_(Ref(response2)));
        EXPECT_CALL(host_->outlier_detector_,
                    putResult(Upstream::Outlier::Result::EXT_ORIGIN_REQUEST_SUCCESS, _));
        callbacks_->onValue(std::move(response2));
      }));
  upstream_read_filter_->onData(fake_data, false);

  EXPECT_CALL(*upstream_connection_,
              close(Network::ConnectionCloseType::NoFlush));
  EXPECT_CALL(pool_callbacks_, onClose());
  client_->close();
}

TEST_F(TcpClientImplTest, Cancel) {
  InSequence s;

  setup();

  T request1;
  EXPECT_CALL(*encoder_, encode(Ref(request1), _));
  client_->makeRequest(request1);

  onConnected();

  T request2;
  EXPECT_CALL(*encoder_, encode(Ref(request2), _));
  client_->makeRequest(request2);

  client_->cancel();

  Buffer::OwnedImpl fake_data;
  EXPECT_CALL(*decoder_, decode(Ref(fake_data)))
      .WillOnce(Invoke([&](Buffer::Instance &) -> void {
        InSequence s;

        TPtr response1(new T());
        EXPECT_CALL(pool_callbacks_, onResponse_(_)).Times(0);
        EXPECT_CALL(host_->outlier_detector_,
                    putResult(Upstream::Outlier::Result::EXT_ORIGIN_REQUEST_SUCCESS, _));
        callbacks_->onValue(std::move(response1));

        TPtr response2(new T());
        EXPECT_CALL(pool_callbacks_, onResponse_(_)).Times(0);
        EXPECT_CALL(host_->outlier_detector_,
                    putResult(Upstream::Outlier::Result::EXT_ORIGIN_REQUEST_SUCCESS, _));
        callbacks_->onValue(std::move(response2));
      }));
  upstream_read_filter_->onData(fake_data, false);

  EXPECT_CALL(*upstream_connection_,
              close(Network::ConnectionCloseType::NoFlush));
  client_->close();

  // TODO(talnordan): What should be counted as a canceled request?
  EXPECT_EQ(0UL, host_->cluster_.stats_.upstream_rq_cancelled_.value());
}

TEST_F(TcpClientImplTest, FailAll) {
  InSequence s;

  setup();

  NiceMock<Network::MockConnectionCallbacks> connection_callbacks;
  client_->addConnectionCallbacks(connection_callbacks);

  T request1;
  EXPECT_CALL(*encoder_, encode(Ref(request1), _));
  client_->makeRequest(request1);

  onConnected();

  EXPECT_CALL(host_->outlier_detector_,
              putResult(Upstream::Outlier::Result::EXT_ORIGIN_REQUEST_FAILED, _));
  EXPECT_CALL(pool_callbacks_, onClose());
  EXPECT_CALL(connection_callbacks,
              onEvent(Network::ConnectionEvent::RemoteClose));
  upstream_connection_->raiseEvent(Network::ConnectionEvent::RemoteClose);

  // TODO(talnordan): What should be counted as an active request?
  EXPECT_EQ(0UL,
            host_->cluster_.stats_.upstream_cx_destroy_with_active_rq_.value());
  EXPECT_EQ(0UL, host_->cluster_.stats_
                     .upstream_cx_destroy_remote_with_active_rq_.value());
}

TEST_F(TcpClientImplTest, FailAllWithCancel) {
  InSequence s;

  setup();

  NiceMock<Network::MockConnectionCallbacks> connection_callbacks;
  client_->addConnectionCallbacks(connection_callbacks);

  T request1;
  EXPECT_CALL(*encoder_, encode(Ref(request1), _));
  client_->makeRequest(request1);

  onConnected();
  client_->cancel();

  EXPECT_CALL(pool_callbacks_, onClose()).Times(0);
  EXPECT_CALL(connection_callbacks,
              onEvent(Network::ConnectionEvent::LocalClose));
  upstream_connection_->raiseEvent(Network::ConnectionEvent::LocalClose);

  // TODO(talnordan): What should be counted as an active request or a canceled
  // one?
  EXPECT_EQ(0UL,
            host_->cluster_.stats_.upstream_cx_destroy_with_active_rq_.value());
  EXPECT_EQ(
      0UL,
      host_->cluster_.stats_.upstream_cx_destroy_local_with_active_rq_.value());
  EXPECT_EQ(0UL, host_->cluster_.stats_.upstream_rq_cancelled_.value());
}

TEST_F(TcpClientImplTest, ProtocolError) {
  InSequence s;

  setup();

  T request1;
  EXPECT_CALL(*encoder_, encode(Ref(request1), _));
  client_->makeRequest(request1);

  onConnected();

  Buffer::OwnedImpl fake_data;
  EXPECT_CALL(*decoder_, decode(Ref(fake_data)))
      .WillOnce(Invoke(
          [&](Buffer::Instance &) -> void { throw ProtocolError("error"); }));
  EXPECT_CALL(host_->outlier_detector_,
              putResult(Upstream::Outlier::Result::EXT_ORIGIN_REQUEST_FAILED, _));
  EXPECT_CALL(*upstream_connection_,
              close(Network::ConnectionCloseType::NoFlush));

  EXPECT_CALL(pool_callbacks_, onClose());

  upstream_read_filter_->onData(fake_data, false);

  EXPECT_EQ(1UL, host_->cluster_.stats_.upstream_cx_protocol_error_.value());
}

TEST_F(TcpClientImplTest, ConnectFail) {
  InSequence s;

  setup();

  T request1;
  EXPECT_CALL(*encoder_, encode(Ref(request1), _));
  client_->makeRequest(request1);

  EXPECT_CALL(host_->outlier_detector_,
              putResult(Upstream::Outlier::Result::EXT_ORIGIN_REQUEST_FAILED, _));

  EXPECT_CALL(pool_callbacks_, onClose());

  upstream_connection_->raiseEvent(Network::ConnectionEvent::RemoteClose);

  EXPECT_EQ(1UL, host_->cluster_.stats_.upstream_cx_connect_fail_.value());
  EXPECT_EQ(1UL, host_->stats_.cx_connect_fail_.value());
}

class ConfigOutlierDisabled : public Config {
  bool disableOutlierEvents() const override { return true; }
};

TEST_F(TcpClientImplTest, OutlierDisabled) {
  InSequence s;

  setup(std::make_unique<ConfigOutlierDisabled>());

  T request1;
  EXPECT_CALL(*encoder_, encode(Ref(request1), _));
  client_->makeRequest(request1);

  EXPECT_CALL(host_->outlier_detector_, putResult(_, _)).Times(0);

  EXPECT_CALL(pool_callbacks_, onClose());

  upstream_connection_->raiseEvent(Network::ConnectionEvent::RemoteClose);

  EXPECT_EQ(1UL, host_->cluster_.stats_.upstream_cx_connect_fail_.value());
  EXPECT_EQ(1UL, host_->stats_.cx_connect_fail_.value());
}

TEST(TcpClientFactoryImplTest, Basic) {
  ClientFactoryImpl<T, MockEncoder, MockDecoder> factory;
  Upstream::MockHost::MockCreateConnectionData conn_info;
  conn_info.connection_ = new NiceMock<Network::MockClientConnection>();
  std::shared_ptr<Upstream::MockHost> host(new NiceMock<Upstream::MockHost>());
  EXPECT_CALL(*host, createConnection_(_, _)).WillOnce(Return(conn_info));
  NiceMock<Event::MockDispatcher> dispatcher;
  MockPoolCallbacks callbacks;
  ConfigImpl config;
  ClientPtr<T> client = factory.create(host, dispatcher, callbacks, config);
  EXPECT_CALL(callbacks, onClose());
  client->close();
}

class TcpConnPoolImplTest : public testing::Test, public ClientFactory<T> {
public:
  TcpConnPoolImplTest() {
    conn_pool_.reset(new InstanceImpl<T, MockDecoder>(cluster_name_, cm_, *this,
                                                      dispatcher_));
    conn_pool_->setPoolCallbacks(callbacks_);
  }

  // Tcp::ConnPoolNats::ClientFactory
  // TODO(talnordan): Use `MockClientFactory` instead of having this class
  // implemnting `ClientFactory<T>.
  ClientPtr<T> create(Upstream::HostConstSharedPtr host, Event::Dispatcher &,
                      PoolCallbacks<T> &, const Config &) override {
    return ClientPtr<T>{create_(host)};
  }

  MOCK_METHOD1(create_, Client<T> *(Upstream::HostConstSharedPtr host));

  const std::string cluster_name_{"foo"};
  NiceMock<Upstream::MockClusterManager> cm_;
  MockPoolCallbacks callbacks_;
  NiceMock<Event::MockDispatcher> dispatcher_;
  InstancePtr<T> conn_pool_;
};

TEST_F(TcpConnPoolImplTest, Basic) {
  InSequence s;

  T value;
  MockClient *client = new NiceMock<MockClient>();

  EXPECT_CALL(cm_.thread_local_cluster_.lb_, chooseHost(_))
      .WillOnce(Invoke([&](Upstream::LoadBalancerContext *context)
                           -> Upstream::HostConstSharedPtr {
        EXPECT_EQ(context->computeHashKey().value(),
                  std::hash<std::string>()("foo"));
        return cm_.thread_local_cluster_.lb_.host_;
      }));
  EXPECT_CALL(*this, create_(_)).WillOnce(Return(client));
  EXPECT_CALL(*client, makeRequest(Ref(value))).Times(1);
  conn_pool_->makeRequest("foo", value);

  // TODO(talnordan): Should `onClose()` be invoked?
  // EXPECT_CALL(callbacks_, onClose());
  EXPECT_CALL(*client, close());
  conn_pool_ = {};
};

TEST_F(TcpConnPoolImplTest, DeleteFollowedByClusterUpdateCallback) {
  conn_pool_.reset();

  std::shared_ptr<Upstream::Host> host(new Upstream::MockHost());
  cm_.thread_local_cluster_.cluster_.prioritySet()
      .getMockHostSet(0)
      ->runCallbacks({}, {host});
}

TEST_F(TcpConnPoolImplTest, NoHost) {
  InSequence s;

  T value;
  EXPECT_CALL(cm_.thread_local_cluster_.lb_, chooseHost(_))
      .WillOnce(Return(nullptr));
  conn_pool_->makeRequest("foo", value);

  conn_pool_ = {};
}

TEST_F(TcpConnPoolImplTest, RemoteClose) {
  InSequence s;

  T value;
  MockClient *client = new NiceMock<MockClient>();

  EXPECT_CALL(cm_.thread_local_cluster_.lb_, chooseHost(_));
  EXPECT_CALL(*this, create_(_)).WillOnce(Return(client));
  EXPECT_CALL(*client, makeRequest(Ref(value))).Times(1);
  conn_pool_->makeRequest("foo", value);

  EXPECT_CALL(dispatcher_, deferredDelete_(_));
  client->raiseEvent(Network::ConnectionEvent::RemoteClose);

  conn_pool_ = {};
}

} // namespace ConnPoolNats
} // namespace Tcp
} // namespace Envoy
