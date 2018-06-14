#include "common/protobuf/utility.h"

#include "authorize.pb.h"
#include "gtest/gtest.h"

namespace Envoy {
namespace Filter {

TEST(ClientCertificateRestrictionTest, Proto) {
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

} // namespace Filter
} // namespace Envoy
