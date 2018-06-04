#include "extensions/filters/network/client_certificate_restriction/client_certificate_restriction.h"

#include "envoy/buffer/buffer.h"
#include "envoy/network/connection.h"

#include "common/common/assert.h"

namespace Envoy {
namespace Filter {

Network::FilterStatus
ClientCertificateRestrictionFilter::onData(Buffer::Instance &, bool) {
  return Network::FilterStatus::Continue;
}

Network::FilterStatus ClientCertificateRestrictionFilter::onNewConnection() {
  ENVOY_CONN_LOG(trace, "client_certificate_restriction: new connection",
                 read_callbacks_->connection());
  // If this is not an SSL connection, do no further checking. High layers
  // should redirect, etc. if SSL is required. Otherwise we need to wait for
  // handshake to be complete before proceeding.
  return (read_callbacks_->connection().ssl())
             ? Network::FilterStatus::StopIteration
             : Network::FilterStatus::Continue;
}

void ClientCertificateRestrictionFilter::onEvent(
    Network::ConnectionEvent event) {
  if (event != Network::ConnectionEvent::Connected) {
    return;
  }

  ASSERT(read_callbacks_->connection().ssl());

  // TODO(talnordan): This is a dummy implementation that simply validates that
  // a peer certificate exists and has a subject. A future implementation should
  // validate the subject against a whitelist retrieved from config.
  // TODO(talnordan): Remove tracing.
  std::string subject{
      read_callbacks_->connection().ssl()->subjectPeerCertificate()};
  ENVOY_CONN_LOG(trace, "client_certificate_restriction: subject is {}",
                 read_callbacks_->connection(), subject);
  if (subject.empty()) {
    read_callbacks_->connection().close(Network::ConnectionCloseType::NoFlush);
    return;
  }

  read_callbacks_->continueReading();
}

} // namespace Filter
} // namespace Envoy
