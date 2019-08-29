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
        ":body_header_transformer_lib",
        ":inja_transformer_lib",
        ":transformer_lib",
        "//api/envoy/config/filter/http/transformation/v2:transformation_filter_proto_cc",
        "@envoy//include/envoy/router:router_interface",
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
        ":transformation_filter_config",
        ":transformer_lib",
        "//source/common/http:solo_filter_utility_lib",
        "//source/extensions/filters/http:solo_well_known_names",
        "@envoy//source/common/common:enum_to_int",
        "@envoy//source/common/config:metadata_lib",
        "@envoy//source/common/http:utility_lib",
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
        ":transformer_lib",
        "@envoy//include/envoy/buffer:buffer_interface",
        "@envoy//source/common/http:header_map_lib",
        "@json//:json-lib",
    ],
)

envoy_cc_library(
    name = "inja_transformer_lib",
    srcs = [
        "inja_transformer.cc",
    ],
    hdrs = [
        "inja_transformer.h",
    ],
    repository = "@envoy",
    deps = [
        ":transformer_lib",
        "//api/envoy/config/filter/http/transformation/v2:transformation_filter_proto_cc",
        "@envoy//include/envoy/buffer:buffer_interface",
        "@envoy//include/envoy/http:header_map_interface",
        "@envoy//source/common/common:macros",
        "@envoy//source/common/protobuf",
        "@inja//:inja-lib",
        "@json//:json-lib",
    ],
)

envoy_cc_library(
    name = "transformer_lib",
    hdrs = [
        "transformer.h",
    ],
    repository = "@envoy",
    deps = [
        "@envoy//include/envoy/buffer:buffer_interface",
        "@envoy//include/envoy/http:header_map_interface",
    ],
)

envoy_cc_library(
    name = "transformation_filter_config_lib",
    srcs = ["transformation_filter_config_factory.cc"],
    hdrs = ["transformation_filter_config_factory.h"],
    repository = "@envoy",
    deps = [
        ":transformation_filter_lib",
        "@envoy//include/envoy/registry",
        "@envoy//source/common/config:json_utility_lib",
        "@envoy//source/extensions/filters/http/common:empty_http_filter_config_lib",
        "@envoy//source/extensions/filters/http/common:factory_base_lib",
    ],
)
