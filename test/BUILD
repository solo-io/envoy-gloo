licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_package",
)

envoy_package()

envoy_cc_test(
    name = "lambda_filter_integration_test",
    srcs = ["lambda_filter_integration_test.cc"],
    data = [":envoy-test.conf"],
    repository = "@envoy",
    deps = [
        "//:lambda_filter_config",
        "@envoy//test/integration:http_integration_lib",
        "@envoy//test/integration:integration_lib",
    ],
)
