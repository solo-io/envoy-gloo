#include "extensions/filters/network/client_certificate_restriction/client_certificate_restriction.h"

#include "envoy/buffer/buffer.h"
#include "envoy/network/connection.h"

#include "common/common/assert.h"
#include "common/ssl/ssl_socket.h"

namespace Envoy {
namespace Filter {

ClientCertificateRestrictionFilter::ClientCertificateRestrictionFilter(
    Upstream::ClusterManager &cm)
    : cm_(cm) {
  UNREFERENCED_PARAMETER(cm_);
}

Network::FilterStatus
ClientCertificateRestrictionFilter::onData(Buffer::Instance &, bool) {
  return Network::FilterStatus::Continue;
}

Network::FilterStatus ClientCertificateRestrictionFilter::onNewConnection() {
  bool ssl{read_callbacks_->connection().ssl()};
  ENVOY_CONN_LOG(trace,
                 "client_certificate_restriction: new connection. ssl={}",
                 read_callbacks_->connection(), ssl);
  // If this is not an SSL connection, do no further checking. High layers
  // should redirect, etc. if SSL is required. Otherwise we need to wait for
  // handshake to be complete before proceeding.
  return (ssl) ? Network::FilterStatus::StopIteration
               : Network::FilterStatus::Continue;
}

void ClientCertificateRestrictionFilter::onEvent(
    Network::ConnectionEvent event) {
  if (event != Network::ConnectionEvent::Connected) {
    return;
  }

  auto &&connection{read_callbacks_->connection()};
  ASSERT(connection.ssl());

  // TODO(talnordan): This is a dummy implementation that simply extracts the
  // URI SAN and the serial number, and validates that the latter exists and is
  // non-empty. A future implementation should validate both against the
  // Authorize API.
  // TODO(talnordan): Convert the serial number to colon-hex-encoded formatting.
  std::string uri_san{connection.ssl()->uriSanPeerCertificate()};
  std::string serial_number{getSerialNumber()};
  if (serial_number.empty()) {
    connection.close(Network::ConnectionCloseType::NoFlush);
    return;
  }

  // TODO(talnordan): Remove tracing.
  ENVOY_CONN_LOG(
      error,
      "client_certificate_restriction: URI SAN is {}, serial number is {}",
      connection, uri_san, serial_number);
  read_callbacks_->continueReading();
}

std::string ClientCertificateRestrictionFilter::getSerialNumber() const {
  // TODO(talnordan): This is a PoC implementation that assumes the subtype of
  // the `Ssl::Connection` pointer.
  auto &&connection{read_callbacks_->connection()};
  Ssl::Connection *ssl{connection.ssl()};
  Ssl::SslSocket *ssl_socket = dynamic_cast<Ssl::SslSocket *>(ssl);
  if (ssl_socket == nullptr) {
    ENVOY_CONN_LOG(
        error, "client_certificate_restriction: unknown SSL connection type",
        connection);
    return "";
  }

  // TODO(talnordan): Avoid relying on the `rawSslForTest()` function.
  SSL *raw_ssl{ssl_socket->rawSslForTest()};
  bssl::UniquePtr<X509> cert(SSL_get_peer_certificate(raw_ssl));
  if (!cert) {
    ENVOY_CONN_LOG(
        error,
        "client_certificate_restriction: failed to retrieve peer certificate",
        connection);
    return "";
  }

  return getSerialNumber(cert.get());
}

std::string
ClientCertificateRestrictionFilter::getSerialNumber(const X509 *cert) {
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

} // namespace Filter
} // namespace Envoy
