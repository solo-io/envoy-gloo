licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_mock",
    "envoy_cc_test",
    "envoy_package",
)

envoy_package()

envoy_cc_test(
    name = "lambda_filter_test",
    srcs = ["lambda_filter_test.cc"],
    repository = "@envoy",
    deps = [
        ":lambda_mocks",
        "//:filter_lib",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/server:server_mocks",
        "@envoy//test/mocks/upstream:upstream_mocks",
        "@envoy//test/test_common:utility_lib",
    ],
)

envoy_cc_test(
    name = "aws_authenticator_test",
    srcs = ["aws_authenticator_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/extensions/filters/http/aws:aws_authenticator_lib",
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

envoy_cc_mock(
    name = "lambda_mocks",
    srcs = ["mocks.cc"],
    hdrs = ["mocks.h"],
    repository = "@envoy",
    deps = [
        "//source/extensions/filters/http/aws:function_retriever_lib",
    ],
)

envoy_cc_test(
    name = "lambda_filter_integration_test",
    srcs = ["lambda_filter_integration_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/extensions/filters/http/aws:lambda_filter_config_lib",
        "@envoy//test/integration:http_integration_lib",
        "@envoy//test/integration:integration_lib",
    ],
)
