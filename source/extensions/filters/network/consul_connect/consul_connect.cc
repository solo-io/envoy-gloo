#include "extensions/filters/network/consul_connect/consul_connect.h"

#include "envoy/buffer/buffer.h"
#include "envoy/network/connection.h"

#include "common/common/assert.h"
#include "common/http/message_impl.h"
#include "common/ssl/ssl_socket.h"

#include "authorize.pb.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace ConsulConnect {

const std::string Filter::AUTHORIZE_PATH = "/v1/agent/connect/authorize";

Config::Config(
    const envoy::config::filter::network::consul_connect::v2::ConsulConnect
        &config)
    : target_(config.target()),
      authorize_hostname_(config.authorize_hostname()),
      authorize_cluster_name_(config.authorize_cluster_name()),
      request_timeout_(
          PROTOBUF_GET_MS_OR_DEFAULT(config, request_timeout, 1000)) {}

Filter::Filter(ConfigSharedPtr config, Upstream::ClusterManager &cm)
    : config_(config), cm_(cm) {}

Network::FilterStatus Filter::onData(Buffer::Instance &, bool) {
  return has_been_authorized_ ? Network::FilterStatus::Continue
                              : Network::FilterStatus::StopIteration;
}

Network::FilterStatus Filter::onNewConnection() {
  return Network::FilterStatus::StopIteration;
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
  std::string uri_san{connection.ssl()->uriSanPeerCertificate()};
  std::string serial_number{toColonHex(getSerialNumber())};
  if (uri_san.empty() || serial_number.empty()) {
    ENVOY_CONN_LOG(trace, "consul_connect: Authorize REST not called",
                   connection);
    closeConnection();
    return;
  }

  // TODO(talnordan): Remove tracing.
  std::string payload{getPayload(config_->target(), uri_san, serial_number)};
  ENVOY_CONN_LOG(error, "consul_connect: payload is {}", connection, payload);

  auto &&authorize_host{config_->authorizeHostname()};
  Http::MessagePtr request{getRequest(authorize_host, payload)};
  auto &&request_timeout{config_->requestTimeout()};

  auto &&authorize_cluster_name{config_->authorizeClusterName()};
  auto &&http_async_client{
      cm_.httpAsyncClientForCluster(authorize_cluster_name)};

  in_flight_request_ =
      http_async_client.send(std::move(request), *this, request_timeout);
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
  std::string json{getBodyString(std::move(m))};
  ENVOY_CONN_LOG(trace,
                 "consul_connect: Authorize REST call "
                 "succeeded, status={}, body={}",
                 connection, m->headers().Status()->value().c_str(), json);
  status_ = Status::Complete;
  agent::connect::authorize::v1::AuthorizeResponse authorize_response;
  const auto status =
      Protobuf::util::JsonStringToMessage(json, &authorize_response);
  if (status.ok() && authorize_response.authorized()) {
    ENVOY_CONN_LOG(trace, "consul_connect: authorized", connection);
    has_been_authorized_ = true;
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

std::string Filter::getSerialNumber() const {
  // TODO(talnordan): This is a PoC implementation that assumes the subtype of
  // the `Ssl::Connection` pointer.
  auto &&connection{read_callbacks_->connection()};
  Ssl::Connection *ssl{connection.ssl()};
  Ssl::SslSocket *ssl_socket = dynamic_cast<Ssl::SslSocket *>(ssl);
  if (ssl_socket == nullptr) {
    ENVOY_CONN_LOG(error, "consul_connect: unknown SSL connection type",
                   connection);
    return "";
  }

  // TODO(talnordan): Avoid relying on the `rawSslForTest()` function.
  SSL *raw_ssl{ssl_socket->rawSslForTest()};
  bssl::UniquePtr<X509> cert(SSL_get_peer_certificate(raw_ssl));
  if (!cert) {
    ENVOY_CONN_LOG(error, "consul_connect: failed to retrieve peer certificate",
                   connection);
    return "";
  }

  return getSerialNumber(cert.get());
}

std::string Filter::getSerialNumber(const X509 *cert) {
  ASSERT(cert);
  ASN1_INTEGER *serial_number = X509_get_serialNumber(const_cast<X509 *>(cert));
  BIGNUM num_bn;
  BN_init(&num_bn);
  ASN1_INTEGER_to_BN(serial_number, &num_bn);
  char *char_serial_number = BN_bn2hex(&num_bn);
  BN_free(&num_bn);
  if (char_serial_number != nullptr) {
    std::string serial_number(char_serial_number);
    OPENSSL_free(char_serial_number);
    return serial_number;
  }
  return "";
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
  return Buffer::BufferUtility::bufferToString(*body);
}

} // namespace ConsulConnect
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
