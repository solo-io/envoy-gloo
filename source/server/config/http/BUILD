licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_binary",
    "envoy_cc_library",
    "envoy_cc_test",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "transformation_filter_config_lib",
    srcs = ["transformation_filter_config_factory.cc"],
    hdrs = ["transformation_filter_config_factory.h"],
    repository = "@envoy",
    deps = [
        "//source/common/http/filter:transformation_filter_lib",
        "@envoy//source/common/config:json_utility_lib",
        "@envoy//source/extensions/filters/http/common:factory_base_lib",
        "@solo_envoy_common//source/common/http:functional_stream_decoder_base_lib",
    ],
)
