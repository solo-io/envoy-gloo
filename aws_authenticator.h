#pragma once

#include <string>
#include "envoy/http/header_map.h"
#include "openssl/sha.h"
#include "envoy/buffer/buffer.h"

namespace Solo {
namespace Lambda {

class AwsAuthenticator {
public:
  AwsAuthenticator(std::string&& access_key, std::string&& secret_key, std::string&& service);  
  
  ~AwsAuthenticator();

  void update_payload_hash(const Buffer::Instance& data);

  void sign(Http::HeaderMap* request_headers, std::list<LowerCaseString>&& headers_to_sign,const std::string& region);

private:
//  void lambdafy();
  const Http::HeaderEntry* get_maybe_inline_header(Http::HeaderMap* request_headers, const LowerCaseString& im);

  static bool lowercasecompare(const LowerCaseString& i,const LowerCaseString& j);

  std::string access_key_;
  std::string first_key_;
  std::string service_;
  std::string host_;


  SHA256_CTX body_sha_;
  
  
  static const std::string ALGORITHM;
  
};

} // Lambda
} // Solo