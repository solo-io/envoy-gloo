#include "extensions/filters/network/consul_connect/consul_connect.h"

#include "envoy/buffer/buffer.h"
#include "envoy/network/connection.h"

#include "common/common/assert.h"
#include "common/common/enum_to_int.h"
#include "common/http/message_impl.h"
#include "common/http/utility.h"

#include "api/envoy/config/filter/network/consul_connect/v2/authorize.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace ConsulConnect {

const std::string Filter::AUTHORIZE_PATH = "/v1/agent/connect/authorize";

Config::Config(
    const envoy::config::filter::network::consul_connect::v2::ConsulConnect
        &config,
    Stats::Scope &scope)
    : target_(config.target()),
      authorize_hostname_(config.authorize_hostname()),
      authorize_cluster_name_(config.authorize_cluster_name()),
      request_timeout_(
          PROTOBUF_GET_MS_OR_DEFAULT(config, request_timeout, 1000)),
      stats_(generateStats(scope)) {}

InstanceStats Config::generateStats(Stats::Scope &scope) {
  const std::string final_prefix{"consul_connect."};
  return {ALL_CONSUL_CONNECT_FILTER_STATS(
      POOL_COUNTER_PREFIX(scope, final_prefix))};
}

Filter::Filter(ConfigSharedPtr config, Upstream::ClusterManager &cm)
    : config_(config), cm_(cm) {}

Network::FilterStatus Filter::onData(Buffer::Instance &, bool) {
  return has_been_authorized_ ? Network::FilterStatus::Continue
                              : Network::FilterStatus::StopIteration;
}

Network::FilterStatus Filter::onNewConnection() {
  // TODO(talnordan): Should `StopIteration` be returned in certain scenarios?
  return Network::FilterStatus::Continue;
}

void Filter::onEvent(Network::ConnectionEvent event) {
  // TODO(talnordan): Refactor into switch-case.
  if (event != Network::ConnectionEvent::Connected) {
    ASSERT(event == Network::ConnectionEvent::RemoteClose ||
           event == Network::ConnectionEvent::LocalClose);
    // TODO(talnordan): Can the use of `status_` be replaced with just comparing
    // `in_flight_request_` to `nullptr`?
    if (status_ == Status::Calling) {
      ASSERT(in_flight_request_ != nullptr);
      in_flight_request_->cancel();
      in_flight_request_ = nullptr;
    }

    return;
  }

  ASSERT(status_ == Status::NotStarted);

  auto &&connection{read_callbacks_->connection()};
  if (!connection.ssl()) {
    closeConnection();
    return;
  }

  // TODO(talnordan): First call `connection.ssl()->peerCertificatePresented()`.
  auto &&ssl = connection.ssl();

  const auto uriSans = ssl->uriSanPeerCertificate();
  std::string uri_san;
  if (!uriSans.empty()) { 
   uri_san = uriSans[0];
  }
  std::string serial_number{ssl->serialNumberPeerCertificate()};
  if (uri_san.empty() || serial_number.empty()) {
    ENVOY_CONN_LOG(trace, "consul_connect: Authorize REST not called",
                   connection);
    closeConnection();
    return;
  }

  // TODO(talnordan): Remove tracing.
  std::string colon_hex_serial_number{toColonHex(serial_number)};
  std::string payload{
      getPayload(config_->target(), uri_san, colon_hex_serial_number)};
  ENVOY_CONN_LOG(error, "consul_connect: payload is {}", connection, payload);

  auto &&authorize_host{config_->authorizeHostname()};
  Http::MessagePtr request{getRequest(authorize_host, payload)};
  auto &&request_timeout{config_->requestTimeout()};

  auto &&authorize_cluster_name{config_->authorizeClusterName()};
  auto &&http_async_client{
      cm_.httpAsyncClientForCluster(authorize_cluster_name)};

  in_flight_request_ = http_async_client.send(
      std::move(request), *this,
      Http::AsyncClient::RequestOptions().setTimeout(request_timeout));
  if (in_flight_request_ == nullptr) {
    // No request could be started. In this case `onFailure()` has already been
    // called inline. Therefore, the connection has already been closed.
    ENVOY_LOG(debug, "consul_connect: can't create request for "
                     "Authorize endpoint");
    return;
  }

  if (status_ == Status::NotStarted) {
    status_ = Status::Calling;
  }
}

void Filter::onSuccess(Http::MessagePtr &&m) {
  in_flight_request_ = nullptr;

  auto &&connection{read_callbacks_->connection()};
  status_ = Status::Complete;
  if (Http::Utility::getResponseStatus(m->headers()) !=
      enumToInt(Http::Code::OK)) {
    ENVOY_CONN_LOG(error,
                   "consul_connect: Authorize REST call "
                   "responded with unexpected status {}",
                   connection, m->headers().Status()->value().c_str());
    closeConnection();
    return;
  }
  std::string json{getBodyString(std::move(m))};
  ENVOY_CONN_LOG(trace,
                 "consul_connect: Authorize REST call "
                 "succeeded, status={}, body={}",
                 connection, m->headers().Status()->value().c_str(), json);
  agent::connect::authorize::v1::AuthorizeResponse authorize_response;
  const auto status =
      Protobuf::util::JsonStringToMessage(json, &authorize_response);
  if (status.ok() && authorize_response.authorized()) {
    ENVOY_CONN_LOG(trace, "consul_connect: authorized", connection);
    has_been_authorized_ = true;
    config_->stats().allowed_.inc();
    read_callbacks_->continueReading();
  } else {
    ENVOY_CONN_LOG(error, "consul_connect: unauthorized", connection);
    closeConnection();
  }
}

void Filter::onFailure(Http::AsyncClient::FailureReason) {
  in_flight_request_ = nullptr;

  // TODO(talnordan): Log reason.
  auto &&connection{read_callbacks_->connection()};
  ENVOY_CONN_LOG(error, "consul_connect: Authorize REST call failed",
                 connection);
  status_ = Status::Complete;
  closeConnection();
}

void Filter::closeConnection() {
  config_->stats().denied_.inc();
  read_callbacks_->connection().close(Network::ConnectionCloseType::NoFlush);
}

std::string Filter::toColonHex(const std::string &s) {
  if (s.empty()) {
    return s;
  }

  // The length of the input string must be even.
  auto &&length = s.length();
  if (length % 2 != 0) {
    return "";
  }

  std::string colon_hex_string;
  colon_hex_string += s[0];
  colon_hex_string += s[1];

  for (size_t i = 2; i < length; i += 2) {
    colon_hex_string += ':';
    colon_hex_string += s[i];
    colon_hex_string += s[i + 1];
  }

  return colon_hex_string;
}

std::string Filter::getPayload(const std::string &target,
                               const std::string &client_cert_uri,
                               const std::string &client_cert_serial) {
  agent::connect::authorize::v1::AuthorizePayload proto_payload{};
  proto_payload.set_target(target);
  proto_payload.set_clientcerturi(client_cert_uri);
  proto_payload.set_clientcertserial(client_cert_serial);

  return MessageUtil::getJsonStringFromMessage(proto_payload);
}

Http::MessagePtr Filter::getRequest(const std::string &host,
                                    const std::string &payload) {
  Http::MessagePtr request(new Http::RequestMessageImpl());
  request->headers().insertContentType().value().setReference(
      Http::Headers::get().ContentTypeValues.Json);
  request->headers().insertPath().value().setReference(AUTHORIZE_PATH);
  request->headers().insertHost().value().setReference(host);
  request->headers().insertMethod().value().setReference(
      Http::Headers::get().MethodValues.Post);
  request->headers().insertContentLength().value(payload.length());
  request->body().reset(new Buffer::OwnedImpl(payload));
  return request;
}

std::string Filter::getBodyString(Http::MessagePtr &&m) {
  Buffer::InstancePtr &body = m->body();
  return body->toString();
}

} // namespace ConsulConnect
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
