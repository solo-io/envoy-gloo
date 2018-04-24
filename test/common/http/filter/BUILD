licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_package",
)

envoy_package()

envoy_cc_test(
    name = "cache_filter_test",
    srcs = [
        "cache_filter_test.cc",
    ],
    repository = "@envoy",
    deps = [
        "//:filter_lib",
        "//test/mocks/cache:cache_mocks",
        "@envoy//test/mocks/buffer:buffer_mocks",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/server:server_mocks",
        "@envoy//test/mocks/upstream:upstream_mocks",
    ],
)
