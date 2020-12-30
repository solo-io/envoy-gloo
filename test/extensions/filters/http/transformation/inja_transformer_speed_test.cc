#include "common/common/empty_string.h"

#include "extensions/filters/http/transformation/inja_transformer.h"

#include "common/matcher/matcher.h"
#include "test/mocks/http/mocks.h"
#include "test/test_common/utility.h"

#include "benchmark/benchmark.h"
#include "fmt/format.h"

using json = nlohmann::json;

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Transformation {

namespace {
std::function<const std::string &()> empty_body = [] { return EMPTY_STRING; };
}

static void BM_ExrtactHeader(benchmark::State &state) {
  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.solo.io"},
                                         {":path", "/users/123"}};
  envoy::api::v2::filter::http::Extraction extractor;
  extractor.set_header(":path");
  extractor.set_regex("/users/(\\d+)");
  extractor.set_subgroup(1);
  size_t output_bytes = 0;
  NiceMock<Http::MockStreamDecoderFilterCallbacks> callbacks;

  Extractor ext(extractor);
  for (auto _ : state) {
    auto view = ext.extract(callbacks, headers, empty_body);
    output_bytes += view.length();
  }
  benchmark::DoNotOptimize(output_bytes);
}
// Register the function as a benchmark
BENCHMARK(BM_ExrtactHeader);

} // namespace Transformation
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy

// Run the benchmark
BENCHMARK_MAIN();
