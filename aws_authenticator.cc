#include <string>
#include <algorithm>
#include <vector>
#include <list>

#include "aws_authenticator.h"

#include "common/http/headers.h"
#include "envoy/http/header_map.h"

#include "common/common/hex.h"
#include "common/common/empty_string.h"
#include "common/common/utility.h"

#include "openssl/digest.h"
#include "openssl/hmac.h"
#include "openssl/sha.h"

namespace Solo {
namespace Lambda {

const std::string AwsAuthenticator::ALGORITHM = "AWS4-HMAC-SHA256";
//TODO: move service to sign function
AwsAuthenticator::AwsAuthenticator(std::string&& access_key, std::string&& secret_key, std::string&& service) : 
  access_key_(access_key), first_key_("AWS4"+secret_key), service_(service) {
    SHA256_Init(&body_sha_);    
}

AwsAuthenticator::~AwsAuthenticator(){

}

void AwsAuthenticator::update_payload_hash(const Buffer::Instance& data) { 

  uint64_t num_slices = data.getRawSlices(nullptr, 0);
  Buffer::RawSlice slices[num_slices];
  data.getRawSlices(slices, num_slices);
  for (Buffer::RawSlice& slice : slices) {
    SHA256_Update(&body_sha_, slice.mem_, slice.len_);
  }

}

const Http::HeaderEntry* AwsAuthenticator::get_maybe_inline_header(Http::HeaderMap* request_headers, const LowerCaseString& im) { 
  #define Q(x) #x
  #define QUOTE(x) Q(x)

  #define CHECK_INLINE_HEADER(name)                                                         \
  static LowerCaseString name##Str = LowerCaseString(std::string(QUOTE(name)), true);                       \
  if (im == name##Str) {                                                                    \
    return request_headers->name();                                                         \
  }                                                       
  
  ALL_INLINE_HEADERS(CHECK_INLINE_HEADER)
  
  return nullptr;
}

bool AwsAuthenticator::lowercasecompare(const LowerCaseString& i,const LowerCaseString& j) { return (i.get() < j.get()); }

void AwsAuthenticator::sign(Http::HeaderMap* request_headers, std::list<LowerCaseString>&& headers, const std::string& region) {
  static LowerCaseString dateheader = LowerCaseString("x-amz-date");
  headers.push_back(dateheader);
  headers.sort(lowercasecompare);

  auto now = std::chrono::system_clock::now();
  std::string RequestDateTime = DateFormatter("%Y%m%dT%H%M%SZ").fromTime(now);
  request_headers->addReferenceKey(dateheader, RequestDateTime);
  

  // insert x-amz-date , 
  // sign Host, as they are a must.
  
  
  std::stringstream  canonicalHeaders;
  std::stringstream  signedHeaders;

  for (auto header = headers.begin(), end = headers.end(); header != end; header++) {  
    const Http::HeaderEntry* headerEntry = request_headers->get(*header);
    if (headerEntry == nullptr) {
      headerEntry = get_maybe_inline_header(request_headers, *header);
    }

    auto headerName = header->get();
    canonicalHeaders << headerName;
    signedHeaders <<  headerName; 
        
    canonicalHeaders << ':';
    if (headerEntry != nullptr) {
          canonicalHeaders << headerEntry->value().c_str();
      // TODO: add warning if null
    }
    canonicalHeaders << '\n';
    std::list<LowerCaseString>::const_iterator next = header;
    next++;
    if (next != end) {
      signedHeaders << ";";
    }
  }
  std::string CanonicalHeaders = canonicalHeaders.str();
  std::string SignedHeaders = signedHeaders.str();
  

  uint8_t payload_out[SHA256_DIGEST_LENGTH];
  SHA256_Final(payload_out, &body_sha_);
  std::string hexpayload = Hex::encode(payload_out, SHA256_DIGEST_LENGTH);

  std::string HTTPRequestMethod = Http::Headers::get().MethodValues.Post;
  const Http::HeaderString& CanonicalURI = request_headers->Path()->value();
  std::string CanonicalQueryString = EMPTY_STRING; // TODO : support query string.
/*
  std::string CanonicalRequest =
  HTTPRequestMethod + '\n' +
  CanonicalURI + '\n' +
  CanonicalQueryString + '\n' +
  CanonicalHeaders + '\n' +
  SignedHeaders + '\n' +
  hexpayload;
*/

  SHA256_CTX cononicalRequestHash;
  SHA256_Init(&cononicalRequestHash);    
  SHA256_Update(&cononicalRequestHash, HTTPRequestMethod.c_str(), HTTPRequestMethod.size());
  SHA256_Update(&cononicalRequestHash, "\n", 1);
  SHA256_Update(&cononicalRequestHash, CanonicalURI.c_str(), CanonicalURI.size());
  SHA256_Update(&cononicalRequestHash, "\n", 1);
  SHA256_Update(&cononicalRequestHash, CanonicalQueryString.c_str(), CanonicalQueryString.size());
  SHA256_Update(&cononicalRequestHash, "\n", 1);
  SHA256_Update(&cononicalRequestHash, CanonicalHeaders.c_str(), CanonicalHeaders.size());
  SHA256_Update(&cononicalRequestHash, "\n", 1);
  SHA256_Update(&cononicalRequestHash, SignedHeaders.c_str(), SignedHeaders.size());
  SHA256_Update(&cononicalRequestHash, "\n", 1);
  SHA256_Update(&cononicalRequestHash, hexpayload.c_str(), hexpayload.size());
  
  uint8_t cononicalRequestHashOut[SHA256_DIGEST_LENGTH];
  SHA256_Final(cononicalRequestHashOut, &cononicalRequestHash);

//  SHA256(static_cast<const uint8_t*>(static_cast<const void*>(CanonicalRequest.c_str())), CanonicalRequest.size(), out);
  std::string hashedCanonicalRequest = Hex::encode(cononicalRequestHashOut, SHA256_DIGEST_LENGTH);
  
  std::string CredentialScopeDate = DateFormatter("%Y%m%d").fromTime(now);

  std::stringstream credentialScopeStream;
  credentialScopeStream << CredentialScopeDate << "/" <<region << "/" << service_ << "/aws4_request";
  std::string CredentialScope = credentialScopeStream.str();

  /*
  auto kDate = doHMAC(tovec("AWS4" + kSecret), tovec(CredentialScopeDate));
  auto kRegion = doHMAC(kDate, tovec(Region));
  auto kService = doHMAC(kRegion, tovec(Service));
  auto kSigning = doHMAC(kService, tovec("aws4_request"));
*/
  auto evp = EVP_sha256();
  unsigned int out_len = EVP_MD_size(evp);
  uint8_t out[out_len];
  
  HMAC_CTX ctx;
  HMAC_Init(&ctx, first_key_.data(), first_key_.size(), evp);
  HMAC_Update(&ctx, reinterpret_cast<const uint8_t*>(CredentialScopeDate.c_str()), CredentialScopeDate.size());
  HMAC_Final(&ctx, out, &out_len);

  HMAC_Init(&ctx, out, out_len, nullptr);
  HMAC_Update(&ctx, reinterpret_cast<const uint8_t*>(region.c_str()), region.size());
  HMAC_Final(&ctx, out, &out_len);

  HMAC_Init(&ctx, out, out_len, nullptr);
  HMAC_Update(&ctx, reinterpret_cast<const uint8_t*>(service_.c_str()), service_.size());
  HMAC_Final(&ctx, out, &out_len);
  
  static std::string aws_request = "aws4_request";
  HMAC_Init(&ctx, out, out_len, nullptr);
  HMAC_Update(&ctx, reinterpret_cast<const uint8_t*>(aws_request.c_str()), aws_request.size());
  HMAC_Final(&ctx, out, &out_len);

  /*

  std::string StringToSign =
  Algorithm + '\n' +
  RequestDateTime + '\n' +
  CredentialScope + '\n' +
  hashedCanonicalRequest;
  */
  HMAC_Init(&ctx, out, out_len, nullptr);
  HMAC_Update(&ctx, reinterpret_cast<const uint8_t*>(ALGORITHM.c_str()), ALGORITHM.size());
  HMAC_Update(&ctx, reinterpret_cast<const uint8_t*>("\n"), 1);
  HMAC_Update(&ctx, reinterpret_cast<const uint8_t*>(RequestDateTime.c_str()), RequestDateTime.size());
  HMAC_Update(&ctx, reinterpret_cast<const uint8_t*>("\n"), 1);
  HMAC_Update(&ctx, reinterpret_cast<const uint8_t*>(CredentialScope.c_str()), CredentialScope.size());
  HMAC_Update(&ctx, reinterpret_cast<const uint8_t*>("\n"), 1);
  HMAC_Update(&ctx, reinterpret_cast<const uint8_t*>(hashedCanonicalRequest.c_str()), hashedCanonicalRequest.size());
  HMAC_Final(&ctx, out, &out_len);

  std::string signature = Hex::encode(out, out_len);

  std::stringstream authorizationvalue;
  authorizationvalue 
      << ALGORITHM << " Credential=" << access_key_ 
      << "/" << CredentialScope << ", SignedHeaders=" 
      << SignedHeaders << ", Signature=" << signature;

   request_headers->insertAuthorization().value(authorizationvalue.str());
   
}

} // Lambda
} // Solo
