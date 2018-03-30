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
    name = "transformation_filter_config",
    srcs = [
        "transformation_filter_config.cc",
    ],
    hdrs = [
        "transformation_filter_config.h",
    ],
    repository = "@envoy",
    deps = [
        "//:transformation_filter_proto_cc",
        "//source/common/config:transformation_well_known_names_lib",
        "@envoy//source/exe:envoy_common_lib",
    ],
)

envoy_cc_library(
    name = "transformation_filter_lib",
    srcs = [
        "transformation_filter.cc",
    ],
    hdrs = [
        "transformation_filter.h",
    ],
    repository = "@envoy",
    deps = [
        "//source/common/http/filter:body_header_transformer_lib",
        "//source/common/http/filter:transformation_filter_config",
        "//source/common/http/filter:transformer_lib",
        "@envoy//source/exe:envoy_common_lib",
        "@solo_envoy_common//include/envoy/http:metadata_accessor_interface",
        "@solo_envoy_common//source/common/http:solo_filter_utility_lib",
    ],
)

envoy_cc_library(
    name = "body_header_transformer_lib",
    srcs = [
        "body_header_transformer.cc",
    ],
    hdrs = [
        "body_header_transformer.h",
    ],
    repository = "@envoy",
    deps = [
        "@envoy//source/common/http:header_map_lib",
        "@envoy//source/exe:envoy_common_lib",
        "@json//:json-lib",
    ],
)

envoy_cc_library(
    name = "transformer_lib",
    srcs = [
        "transformer.cc",
    ],
    hdrs = [
        "transformer.h",
    ],
    repository = "@envoy",
    deps = [
        "//:transformation_filter_proto_cc",
        "@envoy//source/common/protobuf",
        "@envoy//source/exe:envoy_common_lib",
        "@inja//:inja-lib",
        "@json//:json-lib",
    ],
)
