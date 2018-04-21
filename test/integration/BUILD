licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_binary",
    "envoy_cc_library",
    "envoy_cc_test",
    "envoy_package",
)

envoy_package()

envoy_cc_test(
    name = "cache_filter_integration_test",
    srcs = ["cache_filter_integration_test.cc"],
    data = [":envoy-test.conf"],
    repository = "@envoy",
    deps = [
        "//source/server/config/http:cache_filter_config_lib",
        "@envoy//test/integration:http_integration_lib",
        "@envoy//test/integration:integration_lib",
    ],
)
