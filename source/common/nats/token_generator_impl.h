#pragma once

#include <string>

#include "include/envoy/nats/token_generator.h"
#include "envoy/runtime/runtime.h"

#include "source/common/nats/nuid/nuid.h"

namespace Envoy {
namespace Nats {

class TokenGeneratorImpl : public TokenGenerator {
public:
  explicit TokenGeneratorImpl(Random::RandomGenerator &random_generator);

  // Nats::TokenGenerator
  std::string random() override;

private:
  Nuid::Nuid nuid_;
};

} // namespace Nats
} // namespace Envoy
