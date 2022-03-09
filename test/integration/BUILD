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
    name = "aws_lambda_filter_integration_test",
    srcs = ["aws_lambda_filter_integration_test.cc"],
    repository = "@envoy",
    data = [
        "fakejwt.txt",
    ],
    deps = [
        "//source/extensions/filters/http/aws_lambda:aws_lambda_filter_config_lib",
        "//source/extensions/filters/http/transformation:transformation_filter_config_lib",
        "@envoy//test/integration:http_integration_lib",
        "@envoy//test/integration:integration_lib",
    ],
)

envoy_gloo_cc_test(
    name = "transformation_filter_integration_test",
    srcs = ["transformation_filter_integration_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/extensions/filters/http/transformation:transformation_filter_config_lib",
        "//source/extensions/filters/http:solo_well_known_names",
        "@envoy//test/integration:http_integration_lib",
        "@envoy//test/integration:integration_lib",
    ],
)
