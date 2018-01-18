licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_package",
)

envoy_package()

envoy_cc_test(
    name = "lambda_filter_config_test",
    srcs = ["lambda_filter_config_test.cc"],
    repository = "@envoy",
    deps = [
        "//:lambda_filter_config_factory",
        "@envoy//test/test_common:utility_lib",
    ],
)

envoy_cc_test(
    name = "lambda_filter_integration_test",
    srcs = ["lambda_filter_integration_test.cc"],
    data = [":envoy-test.conf"],
    repository = "@envoy",
    deps = [
        "//:lambda_filter_config_factory",
        "@envoy//test/integration:http_integration_lib",
        "@envoy//test/integration:integration_lib",
    ],
)

envoy_cc_test(
    name = "lambda_filter_lib_test",
    srcs = ["map_function_retriever_test.cc"],
    repository = "@envoy",
    deps = [
        "//:lambda_filter_lib",
        "@envoy//test/test_common:utility_lib",
    ],
)
