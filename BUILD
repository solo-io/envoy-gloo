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

envoy_cc_binary(
    name = "envoy",
    repository = "@envoy",
    deps = [
        ":filter_lib",
        "@envoy//source/exe:envoy_main_entry_lib",
    ],
)

api_proto_library(
    name = "lambda_filter_proto",
    srcs = ["lambda_filter.proto"],
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
    name = "lambda_filter_config",
    hdrs = [
        "common/config/solo_well_known_names.h",
        "lambda_filter_config.h",
    ],
    repository = "@envoy",
    deps = [
        ":lambda_filter_proto_cc",
        "@envoy//source/exe:envoy_common_lib",
    ],
)

envoy_cc_library(
    name = "lambda_filter_lib",
    srcs = [
        "lambda_filter.cc",
        "map_function_retriever.cc",
        "metadata_function_retriever.cc",
        "solo_filter_utility.cc",
    ],
    hdrs = [
        "function.h",
        "function_retriever.h",
        "lambda_filter.h",
        "map_function_retriever.h",
        "metadata_function_retriever.h",
        "solo_filter_utility.h",
    ],
    repository = "@envoy",
    deps = [
        ":aws_authenticator_lib",
        ":lambda_filter_config",
        "@envoy//source/exe:envoy_common_lib",
    ],
)

envoy_cc_library(
    name = "filter_lib",
    srcs = ["lambda_filter_config_factory.cc"],
    hdrs = ["lambda_filter_config_factory.h"],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        ":lambda_filter_lib",
        "@envoy//source/exe:envoy_common_lib",
    ],
)
