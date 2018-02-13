licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_package",
)

envoy_package()

envoy_cc_test(
    name = "lambda_filter_test",
    srcs = ["lambda_filter_test.cc"],
    repository = "@envoy",
    deps = [
        "//:filter_lib",
        "//test/mocks/lambda:lambda_mocks",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/server:server_mocks",
        "@envoy//test/mocks/upstream:upstream_mocks",
        "@envoy//test/test_common:utility_lib",
    ],
)

envoy_cc_test(
    name = "metadata_function_retriever_test",
    srcs = ["metadata_function_retriever_test.cc"],
    repository = "@envoy",
    deps = [
        "//:filter_lib",
        "@envoy//test/test_common:utility_lib",
    ],
)
