licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_package",
)
load(
    "@envoy_gloo//bazel:envoy_test.bzl",
    "envoy_gloo_cc_test",
)

envoy_package()

envoy_gloo_cc_test(
    name = "api_gateway_transformer_test",
    srcs = ["transformer_test.cc"],
    repository = "@envoy",
    deps = [
        "//api/envoy/config/transformer/aws_lambda/v2:pkg_cc_proto",
        "//source/extensions/transformers/aws_lambda:api_gateway_transformer_lib",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/server:server_mocks",
        "@envoy//test/test_common:utility_lib",
    ],
)