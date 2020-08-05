#include "common/nats/streaming/client_pool.h"

#include "common/nats/codec_impl.h"
#include "common/nats/streaming/client_impl.h"
#include "common/tcp/conn_pool_impl.h"

namespace Envoy {
namespace Nats {
namespace Streaming {

ClientPool::ClientPool(
    const std::string &cluster_name, Upstream::ClusterManager &cm,
    Tcp::ConnPoolNats::ClientFactory<Message> &client_factory,
    ThreadLocal::SlotAllocator &tls, Random::RandomGenerator &random,
    const std::chrono::milliseconds &op_timeout)
    : cm_(cm), client_factory_(client_factory), slot_(tls.allocateSlot()),
      random_(random), op_timeout_(op_timeout) {
  slot_->set([this, cluster_name](Event::Dispatcher &dispatcher)
                 -> ThreadLocal::ThreadLocalObjectSharedPtr {
    Tcp::ConnPoolNats::InstancePtr<Message> conn_pool(
        new Tcp::ConnPoolNats::InstanceImpl<Message, DecoderImpl>(
            cluster_name, cm_, client_factory_, dispatcher));
    return std::make_shared<ThreadLocalPool>(std::move(conn_pool), random_,
                                             dispatcher, op_timeout_);
  });
}

PublishRequestPtr ClientPool::makeRequest(const std::string &subject,
                                          const std::string &cluster_id,
                                          const std::string &discover_prefix,
                                          std::string &&payload,
                                          PublishCallbacks &callbacks) {
  return slot_->getTyped<ThreadLocalPool>().getClient().makeRequest(
      subject, cluster_id, discover_prefix, std::move(payload), callbacks);
}

ClientPool::ThreadLocalPool::ThreadLocalPool(
    Tcp::ConnPoolNats::InstancePtr<Message> &&conn_pool,
    Random::RandomGenerator &random, Event::Dispatcher &dispatcher,
    const std::chrono::milliseconds &op_timeout)
    : client_(std::move(conn_pool), random, dispatcher, op_timeout) {}

Client &ClientPool::ThreadLocalPool::getClient() { return client_; }

} // namespace Streaming
} // namespace Nats
} // namespace Envoy
