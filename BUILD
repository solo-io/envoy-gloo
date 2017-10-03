package(default_visibility = ["//visibility:public"])

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_binary",
    "envoy_cc_library",
    "envoy_cc_test",
)

envoy_cc_binary(
    name = "envoy",
    repository = "@envoy",
    deps = [
        ":lambda_filter_config",
        "@envoy//source/exe:envoy_main_entry_lib",
    ],
)


envoy_cc_library(
    name = "aws_authenticator_lib",
    srcs = ["aws_authenticator.cc"],
    hdrs = ["aws_authenticator.h"],
    repository = "@envoy",
    deps = [
        "@envoy//source/exe:envoy_common_lib",
    ],
)

envoy_cc_library(
    name = "lambda_filter_lib",
    srcs = ["lambda_filter.cc"],
    hdrs = ["lambda_filter.h"],
    repository = "@envoy",
    deps = [
        ":aws_authenticator_lib",        
        "@envoy//source/exe:envoy_common_lib",
    ],
)

envoy_cc_library(
    name = "lambda_filter_config",
    srcs = ["lambda_filter_config.cc"],
    repository = "@envoy",
    deps = [
        ":lambda_filter_lib",
        "@envoy//source/exe:envoy_common_lib",
    ],
)

envoy_cc_test(
    name = "lambda_filter_integration_test",
    srcs = ["lambda_filter_integration_test.cc"],
    data = [":envoy.conf"],
    repository = "@envoy",
    deps = [
        ":lambda_filter_config",
        "@envoy//test/integration:integration_lib",
        "@envoy//test/integration:http_integration_lib"
    ],
)
