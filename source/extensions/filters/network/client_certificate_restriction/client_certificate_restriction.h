#pragma once

#include <openssl/x509v3.h>

#include "envoy/network/connection.h"
#include "envoy/network/filter.h"
#include "envoy/upstream/cluster_manager.h"

#include "common/buffer/buffer_utility.h"
#include "common/common/logger.h"

#include "api/envoy/config/filter/network/client_certificate_restriction/v2/client_certificate_restriction.pb.validate.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace ClientCertificateRestriction {

/**
 * Global configuration for client certificate restriction.
 */
class ClientCertificateRestrictionConfig {
public:
  ClientCertificateRestrictionConfig(
      const envoy::config::filter::network::client_certificate_restriction::v2::
          ClientCertificateRestriction &config);

  const std::string &target() { return target_; }
  const std::string &authorizeHostname() { return authorize_hostname_; }
  const std::string &authorizeClusterName() { return authorize_cluster_name_; }
  const std::chrono::milliseconds &requestTimeout() { return request_timeout_; }

private:
  const std::string target_;
  const std::string authorize_hostname_;
  const std::string authorize_cluster_name_;
  const std::chrono::milliseconds request_timeout_;
};

typedef std::shared_ptr<ClientCertificateRestrictionConfig>
    ClientCertificateRestrictionConfigSharedPtr;

/**
 * A client SSL certificate restriction filter instance. One per connection.
 */
class ClientCertificateRestrictionFilter
    : public Network::ReadFilter,
      public Network::ConnectionCallbacks,
      public Http::AsyncClient::Callbacks,
      Logger::Loggable<Logger::Id::filter> {
public:
  ClientCertificateRestrictionFilter(
      ClientCertificateRestrictionConfigSharedPtr config,
      Upstream::ClusterManager &cm);

  // Network::ReadFilter
  Network::FilterStatus onData(Buffer::Instance &data,
                               bool end_stream) override;
  Network::FilterStatus onNewConnection() override;
  void initializeReadFilterCallbacks(
      Network::ReadFilterCallbacks &callbacks) override {
    read_callbacks_ = &callbacks;
    read_callbacks_->connection().addConnectionCallbacks(*this);
  }

  // Network::ConnectionCallbacks
  void onEvent(Network::ConnectionEvent event) override;
  void onAboveWriteBufferHighWatermark() override {}
  void onBelowWriteBufferLowWatermark() override {}

  // Http::AsyncClient::Callbacks
  void onSuccess(Http::MessagePtr &&) override;
  void onFailure(Http::AsyncClient::FailureReason) override;

private:
  // TODO(talnordan): This is duplicated from `ExtAuthz::Filter`.
  // State of this filter's communication with the external authorization
  // service. The filter has either not started calling the external service, in
  // the middle of calling it or has completed.
  enum class Status { NotStarted, Calling, Complete };

  inline void closeConnection();

  // TODO(talnordan): Open an issue suggesting to make the Authorize enpoint
  // accept serial numbers that are not colon-hex encoded. Such an enhancement
  // to the Authorize endpoint would prevent the filter from having to perform
  // this string manipulation.
  static inline std::string toColonHex(const std::string &s);

  // TODO(talnordan): Consider moving this function to `Ssl::Connection`.
  inline std::string getSerialNumber() const;

  // TODO(talnirdan): This code is duplicated from
  // Ssl::ContextImpl::getSerialNumber().
  static inline std::string getSerialNumber(const X509 *cert);

  static inline std::string getPayload(const std::string &target,
                                       const std::string &client_cert_uri,
                                       const std::string &client_cert_serial);

  static inline Http::MessagePtr getRequest(const std::string &host,
                                            const std::string &payload);

  std::string getBodyString(Http::MessagePtr &&m);

  ClientCertificateRestrictionConfigSharedPtr config_;
  Upstream::ClusterManager &cm_;
  Network::ReadFilterCallbacks *read_callbacks_{};
  Status status_{Status::NotStarted};
  bool has_been_authorized_{};

  // The current in-flight request to the Authorize endpoint if any, otherwise
  // `nullptr`.
  Http::AsyncClient::Request *in_flight_request_{};

  static const std::string AUTHORIZE_PATH;
};

} // namespace ClientCertificateRestriction
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
