licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_binary",
    "envoy_cc_library",
    "envoy_cc_test",
    "envoy_package",
)

envoy_package()

load("@envoy_api//bazel:api_build_system.bzl", "api_proto_library")

envoy_cc_library(
    name = "lambda_filter_config_lib",
    srcs = ["lambda_filter_config_factory.cc"],
    hdrs = ["lambda_filter_config_factory.h"],
    repository = "@envoy",
    deps = [
        "//source/common/http/filter:lambda_filter_lib",
        "@envoy//source/exe:envoy_common_lib",
        "@solo_envoy_common//source/common/http:functional_stream_decoder_base_lib",
    ],
)
