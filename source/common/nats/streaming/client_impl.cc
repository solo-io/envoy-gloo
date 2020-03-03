#include "common/nats/streaming/client_impl.h"

#include "envoy/event/dispatcher.h"

#include "common/buffer/buffer_utility.h"
#include "common/common/assert.h"
#include "common/common/macros.h"
#include "common/common/utility.h"
#include "common/nats/message_builder.h"

namespace Envoy {
namespace Nats {
namespace Streaming {

using Buffer::BufferUtility;

const std::string ClientImpl::INBOX_PREFIX{"_INBOX"};
const std::string ClientImpl::PUB_ACK_PREFIX{"_STAN.acks"};

ClientImpl::ClientImpl(Tcp::ConnPoolNats::InstancePtr<Message> &&conn_pool_,
                       Runtime::RandomGenerator &random,
                       Event::Dispatcher &dispatcher,
                       const std::chrono::milliseconds &op_timeout)
    : conn_pool_(std::move(conn_pool_)), token_generator_(random),
      dispatcher_(dispatcher), op_timeout_(op_timeout),
      heartbeat_inbox_(
          SubjectUtility::randomChild(INBOX_PREFIX, token_generator_)),
      root_inbox_(SubjectUtility::randomChild(INBOX_PREFIX, token_generator_)),
      root_pub_ack_inbox_(
          SubjectUtility::randomChild(PUB_ACK_PREFIX, token_generator_)),
      connect_response_inbox_(
          SubjectUtility::randomChild(root_inbox_, token_generator_)),
      client_id_(token_generator_.random()), sid_(1) {}

PublishRequestPtr ClientImpl::makeRequest(const std::string &subject,
                                          const std::string &cluster_id,
                                          const std::string &discover_prefix,
                                          std::string &&payload,
                                          PublishCallbacks &callbacks) {
  // TODO(talnordan): For a possible performance improvement, consider replacing
  // the random child token with a counter.
  std::string pub_ack_inbox{
      SubjectUtility::randomChild(root_pub_ack_inbox_, token_generator_)};

  switch (state_) {
  case State::NotConnected:
    enqueuePendingRequest(subject, payload, callbacks, pub_ack_inbox);
    cluster_id_.emplace(cluster_id);
    discover_prefix_.emplace(discover_prefix);
    conn_pool_->setPoolCallbacks(*this);
    sendNatsMessage(MessageBuilder::createConnectMessage());
    state_ = State::Connecting;
    break;
  case State::Connecting:
    enqueuePendingRequest(subject, payload, callbacks, pub_ack_inbox);
    break;
  case State::Connected:
    pubPubMsg(subject, payload, callbacks, pub_ack_inbox);
    break;
  }

  PublishRequestPtr request_ptr(
      new PublishRequestCanceler(*this, std::move(pub_ack_inbox)));
  return request_ptr;
}

void ClientImpl::onResponse(Nats::MessagePtr &&value) {
  ENVOY_LOG(trace, "on response: value is\n[{}]", value->asString());

  // Check whether a payload is expected prior to NATS operation extraction.
  // TODO(talnordan): Eventually, we might want `onResponse()` to be passed a
  // single decoded message consisting of both the `MSG` arguments and the
  // payload.
  if (isWaitingForPayload()) {
    onPayload(std::move(value));
  } else {
    onOperation(std::move(value));
  }
}

void ClientImpl::onClose() {
  // TODO(talnordan)
}

void ClientImpl::onFailure(const std::string &error) {
  // TODO(talnordan): Error handling:
  // 1. Fail all things pending: `pending_request_per_inbox_`,
  // `pub_request_per_inbox_`.
  // 2. Do a best effort to gracefully unsubscribe and disconnect from NATS
  // streaming and NATS.
  // 3. Mark the `State` as `State::NotConnected`.
  ENVOY_LOG(error, "on failure: error is\n[{}]", error);
}

void ClientImpl::onConnected(const std::string &pub_prefix) {
  state_ = State::Connected;

  pub_prefix_.emplace(pub_prefix);

  for (auto it = pending_request_per_inbox_.begin();
       it != pending_request_per_inbox_.end(); ++it) {
    auto &&pub_ack_inbox = it->first;
    auto &&pending_request = it->second;
    pubPubMsg(pending_request.subject, pending_request.payload,
              *pending_request.callbacks, pub_ack_inbox);
  }
  pending_request_per_inbox_.clear();
}

void ClientImpl::send(const Message &message) { sendNatsMessage(message); }

void ClientImpl::cancel(const std::string &pub_ack_inbox) {
  if (state_ == State::Connected) {
    PubRequestHandler::onCancel(pub_ack_inbox, pub_request_per_inbox_);
  } else {
    // Remove the pending request with the specified inbox, if such exists.
    pending_request_per_inbox_.erase(pub_ack_inbox);
  }
}

ClientImpl::PublishRequestCanceler::PublishRequestCanceler(
    ClientImpl &parent, std::string &&pub_ack_inbox)
    : parent_(parent), pub_ack_inbox_(pub_ack_inbox) {}

void ClientImpl::PublishRequestCanceler::cancel() {
  parent_.cancel(pub_ack_inbox_);
}

void ClientImpl::onOperation(Nats::MessagePtr &&value) {
  // TODO(talnordan): For better performance, a future decoder implementation
  // might use zero allocation byte parsing. In such case, this function would
  // need to switch over an `enum class` representing the message type. See:
  // https://github.com/nats-io/go-nats/blob/master/parser.go
  // https://youtu.be/ylRKac5kSOk?t=10m46s

  auto delimiters = " \t";
  auto keep_empty_string = false;
  auto tokens =
      StringUtil::splitToken(value->asString(), delimiters, keep_empty_string);

  auto &&op = tokens[0];
  if (absl::EqualsIgnoreCase(op, "INFO")) {
    onInfo(std::move(value));
  } else if (absl::EqualsIgnoreCase(op, "MSG")) {
    onMsg(std::move(tokens));
  } else if (absl::EqualsIgnoreCase(op, "PING")) {
    onPing();
  } else if (absl::EqualsIgnoreCase(op, "+OK")) {
    ENVOY_LOG(error, "on operation: op is [{}], not throwing", op);
  } else {
    // TODO(talnordan): Error handling.
    // TODO(talnordan): Increment error stats.
    ENVOY_LOG(error, "on operation: op is [{}], throwing", op);
    throw ProtocolError("invalid message");
  }
}

void ClientImpl::onPayload(Nats::MessagePtr &&value) {
  std::string &subject = getSubjectWaitingForPayload();
  absl::optional<std::string> &reply_to = getReplyToWaitingForPayload();
  std::string &payload = value->asString();
  if (subject == heartbeat_inbox_) {
    HeartbeatHandler::onMessage(reply_to, payload, *this);
  } else if (subject == connect_response_inbox_) {
    ConnectResponseHandler::onMessage(reply_to, payload, *this);
  } else {
    PubRequestHandler::onMessage(subject, reply_to, payload, *this,
                                 pub_request_per_inbox_);
  }

  // Mark that the payload has been received.
  doneWaitingForPayload();
}

void ClientImpl::onInfo(Nats::MessagePtr &&value) {
  // TODO(talnordan): Process `INFO` options.
  UNREFERENCED_PARAMETER(value);

  // TODO(talnordan): The following behavior is part of the PoC implementation.
  // TODO(talnordan): `UNSUB` before connection shutdown.
  subHeartbeatInbox();
  subReplyInbox();
  subPubAckInbox();
  pubConnectRequest();
}

void ClientImpl::onMsg(std::vector<absl::string_view> &&tokens) {
  auto num_tokens = tokens.size();
  switch (num_tokens) {
  case 4:
    waitForPayload(std::string(tokens[1]), absl::optional<std::string>{});
    break;
  case 5:
    waitForPayload(std::string(tokens[1]),
                   absl::optional<std::string>(std::string(tokens[3])));
    break;
  default:
    // TODO(talnordan): Error handling.
    ENVOY_LOG(error, "on MSG: num_tokens is {}", num_tokens);
    throw ProtocolError("invalid MSG");
  }
}

void ClientImpl::onPing() { pong(); }

void ClientImpl::onTimeout(const std::string &pub_ack_inbox) {
  PubRequestHandler::onTimeout(pub_ack_inbox, pub_request_per_inbox_);
}

void ClientImpl::subInbox(const std::string &subject) {
  sendNatsMessage(MessageBuilder::createSubMessage(subject, sid_));
  ++sid_;
}

void ClientImpl::subChildWildcardInbox(const std::string &parent_subject) {
  std::string child_wildcard{SubjectUtility::childWildcard(parent_subject)};
  subInbox(child_wildcard);
}

void ClientImpl::subHeartbeatInbox() { subInbox(heartbeat_inbox_); }

void ClientImpl::subReplyInbox() { subChildWildcardInbox(root_inbox_); }

void ClientImpl::subPubAckInbox() {
  subChildWildcardInbox(root_pub_ack_inbox_);
}

void ClientImpl::pubConnectRequest() {
  const std::string subject{
      SubjectUtility::join(discover_prefix_.value(), cluster_id_.value())};

  const std::string connect_request_message =
      MessageUtility::createConnectRequestMessage(client_id_, heartbeat_inbox_);

  pubNatsStreamingMessage(subject, connect_response_inbox_,
                          connect_request_message);
}

void ClientImpl::enqueuePendingRequest(const std::string &subject,
                                       const std::string &payload,
                                       PublishCallbacks &callbacks,
                                       const std::string &pub_ack_inbox) {
  PendingRequest pending_request{subject, payload, &callbacks};
  pending_request_per_inbox_.emplace(pub_ack_inbox, std::move(pending_request));
}

void ClientImpl::pubPubMsg(const std::string &subject,
                           const std::string &payload,
                           PublishCallbacks &callbacks,
                           const std::string &pub_ack_inbox) {
  // TODO(talnordan): Consider moving the following logic to
  // `PubRequestHandler`.

  // TODO(talnordan): Consider making `PubRequest` use the RAII pattern, to make
  // sure that the timer is disabled whenever a request is removed from the map,
  // or if an exception is thrown. We might want the `PubRequest` isntance to
  // keep being created on the stack even after such modififcation.
  // TODO(talnordan): Can we create a timer on the stack rather than on the
  // heap?
  Event::TimerPtr timeout_timer = dispatcher_.createTimer(
      [this, pub_ack_inbox]() -> void { onTimeout(pub_ack_inbox); });
  timeout_timer->enableTimer(op_timeout_);
  PubRequest pub_request(&callbacks, std::move(timeout_timer));
  pub_request_per_inbox_.emplace(pub_ack_inbox, std::move(pub_request));

  const std::string pub_subject{
      SubjectUtility::join(pub_prefix_.value(), subject)};

  const std::string guid = token_generator_.random();
  const std::string pub_msg_message =
      MessageUtility::createPubMsgMessage(client_id_, guid, subject, payload);

  pubNatsStreamingMessage(pub_subject, pub_ack_inbox, pub_msg_message);
}

void ClientImpl::pong() {
  sendNatsMessage(MessageBuilder::createPongMessage());
}

inline void ClientImpl::sendNatsMessage(const Message &message) {
  // TODO(talnordan): Manage hash key computation.
  const std::string hash_key;

  conn_pool_->makeRequest(hash_key, message);
}

inline void ClientImpl::pubNatsStreamingMessage(const std::string &subject,
                                                const std::string &reply_to,
                                                const std::string &message) {
  const Message pubMessage =
      MessageBuilder::createPubMessage(subject, reply_to, message);
  sendNatsMessage(pubMessage);
}

} // namespace Streaming
} // namespace Nats
} // namespace Envoy
