licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "api_gateway_transformer_lib",
    srcs = [
        "api_gateway_transformer.cc",
    ],
    hdrs = [
        "api_gateway_transformer.h",
    ],
    repository = "@envoy",
    deps = [
        "//api/envoy/config/transformer/aws_lambda/v2:pkg_cc_proto",
        "@envoy_gloo//source/extensions/filters/http/transformation:transformer_lib",
        "@envoy//envoy/buffer:buffer_interface",
        "@envoy//source/common/http:header_map_lib",
        "@json//:json-lib",
        "@envoy//source/common/common:minimal_logger_lib",
        "@envoy//source/common/common:base64_lib",
        "@envoy_gloo//source/extensions/filters/http/transformation:transformation_filter_config",
        "@envoy_gloo//source/extensions/filters/http/transformation:transformation_filter_config_lib",
        "@envoy_gloo//source/extensions/filters/http/transformation:transformation_filter_lib",
    ],
)