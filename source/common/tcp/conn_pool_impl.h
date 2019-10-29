#pragma once

#include <type_traits>

#include "envoy/event/dispatcher.h"
#include "envoy/network/connection.h"
#include "envoy/tcp/conn_pool_nats.h"
#include "envoy/thread_local/thread_local.h"
#include "envoy/upstream/cluster_manager.h"
#include "envoy/upstream/thread_local_cluster.h"
#include "envoy/upstream/upstream.h"

#include "common/buffer/buffer_impl.h"
#include "common/common/assert.h"
#include "common/network/filter_impl.h"
#include "common/tcp/codec_impl.h"
#include "common/upstream/load_balancer_impl.h"

namespace Envoy {
namespace Tcp {
namespace ConnPoolNats {

class ConfigImpl : public Config {
public:
  bool disableOutlierEvents() const override { return false; }
};

template <typename T>
class ClientImpl : public Client<T>,
                   public DecoderCallbacks<T>,
                   public Network::ConnectionCallbacks {
public:
  static ClientPtr<T>
  create(Upstream::HostConstSharedPtr host, Event::Dispatcher &dispatcher,
         EncoderPtr<T> &&encoder, DecoderFactory<T> &decoder_factory,
         PoolCallbacks<T> &callbacks, const Config &config) {
    std::unique_ptr<ClientImpl> client(new ClientImpl(
        host, std::move(encoder), decoder_factory, callbacks, config));
    client->connection_ =
        host->createConnection(dispatcher, nullptr, nullptr).connection_;
    client->connection_->addConnectionCallbacks(*client);
    client->connection_->addReadFilter(
        Network::ReadFilterSharedPtr{new UpstreamReadFilter(*client)});
    client->connection_->connect();
    client->connection_->noDelay(true);
    return client;
  }

  ~ClientImpl() {
    ASSERT(connection_->state() == Network::Connection::State::Closed);
    host_->cluster().stats().upstream_cx_active_.dec();
    host_->stats().cx_active_.dec();
  }

  // Tcp::ConnPool::Client
  void
  addConnectionCallbacks(Network::ConnectionCallbacks &callbacks) override {
    connection_->addConnectionCallbacks(callbacks);
  }
  void close() override {
    connection_->close(Network::ConnectionCloseType::NoFlush);
  }
  void makeRequest(const T &request) override {
    ASSERT(connection_->state() == Network::Connection::State::Open);

    incRequestStats();
    encoder_->encode(request, encoder_buffer_);
    connection_->write(encoder_buffer_, false);
  }
  void cancel() override {
    // If we get a cancellation, we just mark all pending request as canceled,
    // and then we drop all responses as they come through. There is no reason
    // to blow away the connection when the remote is already responding as fast
    // as possible.
    canceled_ = true;
  }

private:
  struct UpstreamReadFilter : public Network::ReadFilterBaseImpl {
    UpstreamReadFilter(ClientImpl<T> &parent) : parent_(parent) {}

    // Network::ReadFilter
    Network::FilterStatus onData(Buffer::Instance &data, bool) override {
      parent_.onData(data);
      return Network::FilterStatus::Continue;
    }

    ClientImpl &parent_;
  };

  // TODO(talnordan):
  // The current implementation considers the number of TCP messages sent to be
  // the number of requests. Perhaps it would be more accurate to count the
  // number of HTTP requests? An example of a case in which it can make a
  // difference is whether `PING` messages and `PONG` messages at the NATS layer
  // should be counted as requests.
  void incRequestStats() {
    host_->cluster().stats().upstream_rq_total_.inc();
    host_->stats().rq_total_.inc();
  }

  ClientImpl(Upstream::HostConstSharedPtr host, EncoderPtr<T> &&encoder,
             DecoderFactory<T> &decoder_factory, PoolCallbacks<T> &callbacks,
             const Config &config)
      : host_(host), encoder_(std::move(encoder)),
        decoder_(decoder_factory.create(*this)), callbacks_(callbacks),
        config_(config) {
    host->cluster().stats().upstream_cx_total_.inc();
    host->cluster().stats().upstream_cx_active_.inc();
    host->stats().cx_total_.inc();
    host->stats().cx_active_.inc();
  }
  void onData(Buffer::Instance &data) {
    try {
      decoder_->decode(data);
    } catch (ProtocolError &) {
      putOutlierEvent(Upstream::Outlier::Result::EXT_ORIGIN_REQUEST_FAILED);
      host_->cluster().stats().upstream_cx_protocol_error_.inc();
      connection_->close(Network::ConnectionCloseType::NoFlush);
    }
  }
  void putOutlierEvent(Upstream::Outlier::Result result) {
    if (!config_.disableOutlierEvents()) {
      host_->outlierDetector().putResult(result);
    }
  }

  // Tcp::DecoderCallbacks
  void onValue(MessagePtr<T> &&value) override {
    if (!canceled_) {
      callbacks_.onResponse(std::move(value));
    }

    // TODO(talnordan): How should we count these?
    // else {
    //   host_->cluster().stats().upstream_rq_cancelled_.inc();
    // }

    putOutlierEvent(Upstream::Outlier::Result::EXT_ORIGIN_REQUEST_SUCCESS);
  }

  // Network::ConnectionCallbacks
  void onEvent(Network::ConnectionEvent event) override {
    if (event == Network::ConnectionEvent::RemoteClose ||
        event == Network::ConnectionEvent::LocalClose) {
      // TODO(talnordan): How should we count these?
      // if (!pending_requests_.empty()) {
      //   host_->cluster().stats().upstream_cx_destroy_with_active_rq_.inc();
      if (event == Network::ConnectionEvent::RemoteClose) {
        putOutlierEvent(Upstream::Outlier::Result::EXT_ORIGIN_REQUEST_FAILED);
        // host_->cluster()
        //     .stats()
        //     .upstream_cx_destroy_remote_with_active_rq_.inc();
      }
      //   if (event == Network::ConnectionEvent::LocalClose) {
      //     host_->cluster()
      //         .stats()
      //         .upstream_cx_destroy_local_with_active_rq_.inc();
      //   }
      // }

      // TODO(talnordan): How should we count these?
      // if (canceled_) {
      //   while (!pending_requests_.empty()) {
      //     host_->cluster().stats().upstream_rq_cancelled_.inc();
      //   }
      // }

      if (!canceled_) {
        callbacks_.onClose();
      }

    } else if (event == Network::ConnectionEvent::Connected) {
      connected_ = true;
    }

    if (event == Network::ConnectionEvent::RemoteClose && !connected_) {
      host_->cluster().stats().upstream_cx_connect_fail_.inc();
      host_->stats().cx_connect_fail_.inc();
    }
  }
  void onAboveWriteBufferHighWatermark() override {}
  void onBelowWriteBufferLowWatermark() override {}

  Upstream::HostConstSharedPtr host_;
  Network::ClientConnectionPtr connection_;
  EncoderPtr<T> encoder_;
  Buffer::OwnedImpl encoder_buffer_;
  DecoderPtr decoder_;
  PoolCallbacks<T> &callbacks_;
  const Config &config_;
  bool connected_{};
  bool canceled_{};
};

template <typename T, typename E, typename D>
class ClientFactoryImpl : public ClientFactory<T> {
  static_assert(std::is_base_of<Encoder<T>, E>::value,
                "Encoder<T> should be a base of E");
  static_assert(std::is_base_of<Decoder, D>::value,
                "Decoder should be a base of D");

public:
  // Tcp::ConnPool::ClientFactoryImpl
  ClientPtr<T> create(Upstream::HostConstSharedPtr host,
                      Event::Dispatcher &dispatcher,
                      PoolCallbacks<T> &callbacks,
                      const Config &config) override {
    return ClientImpl<T>::create(host, dispatcher, EncoderPtr<T>{new E()},
                                 decoder_factory_, callbacks, config);
  }

  static ClientFactoryImpl<T, E, D> instance_;

private:
  DecoderFactoryImpl<T, D> decoder_factory_;
};

template <typename T, typename E, typename D>
ClientFactoryImpl<T, E, D> ClientFactoryImpl<T, E, D>::instance_;

template <typename T, typename D> class InstanceImpl : public Instance<T> {
public:
  InstanceImpl(const std::string &cluster_name, Upstream::ClusterManager &cm,
               ClientFactory<T> &client_factory, Event::Dispatcher &dispatcher)
      : cm_(cm), client_factory_(client_factory) {
    thread_local_pool_ =
        std::make_shared<ThreadLocalPool>(*this, dispatcher, cluster_name);
  }

  // Tcp::ConnPool::Instance
  void setPoolCallbacks(PoolCallbacks<T> &callbacks) override {
    RELEASE_ASSERT(callbacks_ == nullptr, "");
    callbacks_ = &callbacks;
  }
  void makeRequest(const std::string &hash_key, const T &request) override {
    thread_local_pool_->makeRequest(hash_key, request);
  }

private:
  struct ThreadLocalPool;

  struct ThreadLocalActiveClient : public Network::ConnectionCallbacks {
    ThreadLocalActiveClient(ThreadLocalPool &parent) : parent_(parent) {}

    // Network::ConnectionCallbacks
    void onEvent(Network::ConnectionEvent event) override {
      if (event == Network::ConnectionEvent::RemoteClose ||
          event == Network::ConnectionEvent::LocalClose) {
        ASSERT(parent_.maybe_client_);
        ThreadLocalActiveClientPtr &client_to_delete = parent_.maybe_client_;
        parent_.dispatcher_.deferredDelete(
            std::move(client_to_delete->client_));
        parent_.maybe_client_ = nullptr;
      }
    }
    void onAboveWriteBufferHighWatermark() override {}
    void onBelowWriteBufferLowWatermark() override {}

    ThreadLocalPool &parent_;
    ClientPtr<T> client_;
  };

  typedef std::unique_ptr<ThreadLocalActiveClient> ThreadLocalActiveClientPtr;

  struct ThreadLocalPool : public ThreadLocal::ThreadLocalObject {
    ThreadLocalPool(InstanceImpl &parent, Event::Dispatcher &dispatcher,
                    const std::string &cluster_name)
        : parent_(parent), dispatcher_(dispatcher),
          cluster_name_(cluster_name) {}
    ~ThreadLocalPool() {
      if (maybe_client_) {
        maybe_client_->client_->close();
      }
    }
    void makeRequest(const std::string &hash_key, const T &request) {
      if (!maybe_client_) {
        auto *cluster = parent_.cm_.get(cluster_name_);
        if (!cluster) {
          // TODO(talnordan):
          // parent_.callbacks_->onFailure("no cluster");
          return;
        }
        LbContextImpl lb_context(hash_key);
        Upstream::HostConstSharedPtr host =
            cluster->loadBalancer().chooseHost(&lb_context);
        if (!host) {
          // TODO(talnordan):
          // parent_.callbacks_->onFailure("no host");
          return;
        }

        ThreadLocalActiveClientPtr client{new ThreadLocalActiveClient(*this)};
        RELEASE_ASSERT(parent_.callbacks_ != nullptr, "");
        client->client_ = parent_.client_factory_.create(
            host, dispatcher_, *parent_.callbacks_, parent_.config_);
        client->client_->addConnectionCallbacks(*client);

        maybe_client_ = std::move(client);
      }

      ThreadLocalActiveClientPtr &client = maybe_client_;

      client->client_->makeRequest(request);
    }

    InstanceImpl &parent_;
    Event::Dispatcher &dispatcher_;
    std::string cluster_name_;
    ThreadLocalActiveClientPtr maybe_client_;
    Common::CallbackHandle *local_host_set_member_update_cb_handle_;
  };

  struct LbContextImpl : public Upstream::LoadBalancerContextBase {
    LbContextImpl(const std::string &hash_key)
        : hash_key_(std::hash<std::string>()(hash_key)) {}
    // TODO(danielhochman): convert to HashUtil::xxHash64 when we have a
    // migration strategy. Upstream::LoadBalancerContext
    absl::optional<uint64_t> computeHashKey() override { return hash_key_; }

    const absl::optional<uint64_t> hash_key_;
  };

  Upstream::ClusterManager &cm_;
  ClientFactory<T> &client_factory_;

  // TODO(talnordan): This member can be owned directly rather than using a
  // `shared_ptr<>`.
  std::shared_ptr<ThreadLocalPool> thread_local_pool_;

  ConfigImpl config_;
  PoolCallbacks<T> *callbacks_{};
};

} // namespace ConnPoolNats
} // namespace Tcp
} // namespace Envoy
