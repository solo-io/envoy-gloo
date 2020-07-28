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
    name = "config_test",
    srcs = ["config_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/extensions/filters/http/aws_lambda:aws_lambda_filter_config_lib",
        "@envoy//test/extensions/common/aws:aws_mocks",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/server:server_mocks",
        "@envoy//test/mocks/upstream:upstream_mocks",
        "@envoy//test/test_common:utility_lib",
        "@envoy//test/common/stats:stat_test_utility_lib",
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

envoy_gloo_cc_test(
    name = "sts_credentials_provider_test",
    srcs = ["sts_credentials_provider_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/extensions/filters/http/aws_lambda:sts_credentials_provider_lib",
        "@envoy//test/extensions/common/aws:aws_mocks",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/server:server_mocks",
        "@envoy//test/mocks/upstream:upstream_mocks",
        "@envoy//test/test_common:utility_lib",
        "@envoy//test/test_common:environment_lib",
        "@envoy//test/common/stats:stat_test_utility_lib",
    ],
)