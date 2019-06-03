licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_mock",
    "envoy_package",
)
load(
    "//bazel:envoy_test.bzl",
    "envoy_gloo_cc_test",
)

envoy_package()

envoy_gloo_cc_test(
    name = "aws_lambda_filter_test",
    srcs = ["aws_lambda_filter_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/extensions/filters/http/aws_lambda:aws_lambda_filter_config_lib",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/server:server_mocks",
        "@envoy//test/mocks/upstream:upstream_mocks",
        "@envoy//test/test_common:utility_lib",
    ],
)

envoy_gloo_cc_test(
    name = "aws_authenticator_test",
    srcs = ["aws_authenticator_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/extensions/filters/http/aws_lambda:aws_authenticator_lib",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/server:server_mocks",
        "@envoy//test/mocks/upstream:upstream_mocks",
        "@envoy//test/test_common:utility_lib",
    ],
)
