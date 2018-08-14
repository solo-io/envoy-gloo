#include "common/protobuf/utility.h"

#include "extensions/filters/network/consul_connect/consul_connect.h"

#include "test/mocks/server/mocks.h"

#include "authorize.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::NiceMock;

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace ConsulConnect {

TEST(ConsulConnectConfigTest, Constructor) {
  envoy::config::filter::network::consul_connect::v2::ConsulConnect
      proto_config;

  proto_config.set_target("target");
  proto_config.set_authorize_hostname("example.com");
  proto_config.set_authorize_cluster_name("authorize");
  proto_config.mutable_request_timeout()->set_seconds(6);

  NiceMock<Server::Configuration::MockFactoryContext> context;
  Config config(proto_config, context.scope());
  EXPECT_EQ("target", config.target());
  EXPECT_EQ("example.com", config.authorizeHostname());
  EXPECT_EQ("authorize", config.authorizeClusterName());
  EXPECT_EQ(std::chrono::milliseconds(6000), config.requestTimeout());
  EXPECT_EQ(0U, config.stats().allowed_.value());
  EXPECT_EQ(0U, config.stats().denied_.value());
}

TEST(ConsulConnectTest, AuthorizePayloadProto) {
  agent::connect::authorize::v1::AuthorizePayload proto_payload{};
  proto_payload.set_target("db");
  proto_payload.set_clientcerturi("spiffe://"
                                  "dc1-7e567ac2-551d-463f-8497-f78972856fc1."
                                  "consul/ns/default/dc/dc1/svc/web");
  proto_payload.set_clientcertserial("04:00:00:00:00:01:15:4b:5a:c3:94");

  std::string actual_json_string{
      MessageUtil::getJsonStringFromMessage(proto_payload, true)};

  std::string expected_json_string =
      R"EOF({
 "Target": "db",
 "ClientCertURI": "spiffe://dc1-7e567ac2-551d-463f-8497-f78972856fc1.consul/ns/default/dc/dc1/svc/web",
 "ClientCertSerial": "04:00:00:00:00:01:15:4b:5a:c3:94"
}
)EOF";
  EXPECT_EQ(expected_json_string, actual_json_string);
}

TEST(ConsulConnectTest, AuthorizeResponseProto) {
  agent::connect::authorize::v1::AuthorizeResponse proto_response{};
  proto_response.set_authorized(true);
  proto_response.set_reason("Matched intention: web => db (allow)");

  std::string actual_json_string{
      MessageUtil::getJsonStringFromMessage(proto_response, true)};

  std::string expected_json_string =
      R"EOF({
 "Authorized": true,
 "Reason": "Matched intention: web =\u003e db (allow)"
}
)EOF";
  EXPECT_EQ(expected_json_string, actual_json_string);
}

} // namespace ConsulConnect
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
