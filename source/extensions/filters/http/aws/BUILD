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
    name = "aws_authenticator_lib",
    srcs = ["aws_authenticator.cc"],
    hdrs = ["aws_authenticator.h"],
    external_deps = ["ssl"],
    repository = "@envoy",
    deps = [
        "@envoy//include/envoy/buffer:buffer_interface",
        "@envoy//include/envoy/http:header_map_interface",
        "@envoy//source/common/common:assert_lib",
        "@envoy//source/common/common:empty_string",
        "@envoy//source/common/common:hex_lib",
        "@envoy//source/common/config:json_utility_lib",
        "@envoy//source/common/http:headers_lib",
        "@envoy//source/common/http:utility_lib",
    ],
)

envoy_cc_library(
    name = "function_retriever_lib",
    srcs = [
        "function.cc",
        "metadata_function_retriever.cc",
    ],
    hdrs = [
        "function.h",
        "function_retriever.h",
        "metadata_function_retriever.h",
    ],
    repository = "@envoy",
    deps = [
        "//source/extensions/filters/http:lambda_well_known_names_lib",
        "@solo_envoy_common//include/envoy/http:metadata_accessor_interface",
        "@solo_envoy_common//source/common/config:solo_metadata_lib",
        "@solo_envoy_common//source/common/http:solo_filter_utility_lib",
    ],
)

envoy_cc_library(
    name = "lambda_filter_lib",
    srcs = [
        "lambda_filter.cc",
    ],
    hdrs = [
        "lambda_filter.h",
    ],
    repository = "@envoy",
    deps = [
        ":aws_authenticator_lib",
        ":function_retriever_lib",
        "@envoy//source/common/http:filter_utility_lib",
        "@solo_envoy_common//include/envoy/http:metadata_accessor_interface",
        "@solo_envoy_common//source/common/http:solo_filter_utility_lib",
    ],
)

envoy_cc_library(
    name = "lambda_filter_config_lib",
    srcs = ["lambda_filter_config_factory.cc"],
    hdrs = ["lambda_filter_config_factory.h"],
    repository = "@envoy",
    deps = [
        ":lambda_filter_lib",
        "@envoy//source/extensions/filters/http/common:empty_http_filter_config_lib",
        "@solo_envoy_common//source/common/http:functional_stream_decoder_base_lib",
    ],
)
