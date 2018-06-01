#include "client_certificate_restriction.h"

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
  // TODO(talnordan)
  ENVOY_CONN_LOG(trace, "client_certificate_restriction: new connection",
                 read_callbacks_->connection());
  return Network::FilterStatus::Continue;
}

} // namespace Filter
} // namespace Envoy
