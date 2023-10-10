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
    name = "transformation_filter_config",
    srcs = [
        "transformation_filter_config.cc",
    ],
    hdrs = [
        "transformation_filter_config.h",
    ],
    repository = "@envoy",
    deps = [
        ":filter_config_lib",
        ":transformation_logger_lib",
        ":transformation_factory_lib",
        ":matcher_lib",
        "//source/extensions/filters/http:solo_well_known_names",
        "//api/envoy/config/filter/http/transformation/v2:pkg_cc_proto",
        "@envoy//envoy/router:router_interface",
        "@envoy//envoy/config:typed_config_interface",
        "@envoy//source/common/protobuf:message_validator_lib",
    ],
)
envoy_cc_library(
    name = "transformation_factory_lib",
    srcs = [
        "transformation_factory.cc",
    ],
    hdrs = [
        "transformation_factory.h",
    ],
    repository = "@envoy",
    deps = [
        ":body_header_transformer_lib",
        ":inja_transformer_lib",
        ":transformer_lib",
        ":transformation_logger_lib",
        "//source/extensions/filters/http:solo_well_known_names",
        "//api/envoy/config/filter/http/transformation/v2:pkg_cc_proto",
        "@envoy//envoy/router:router_interface",
        "@envoy//envoy/config:typed_config_interface",
        "@envoy//source/common/protobuf:message_validator_lib",
        "@envoy//source/common/config:utility_lib",
    ],
)

envoy_cc_library(
    name = "matcher_lib",
    srcs = [
        "matcher.cc",
    ],
    hdrs = [
        "matcher.h",
    ],
    repository = "@envoy",
    deps = [
        ":transformer_lib",
        ":transformation_factory_lib",
        "//api/envoy/config/filter/http/transformation/v2:pkg_cc_proto",
        "@envoy//source/common/http/matching:data_impl_lib",
        "@envoy//source/common/matcher:matcher_lib",
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
        "//source/extensions/filters/http:solo_well_known_names",
        "@envoy//source/common/common:enum_to_int",
        "@envoy//source/common/config:metadata_lib",
        "@envoy//source/common/http:header_map_lib",
        "@envoy//source/common/http:utility_lib",
        "@envoy//envoy/stats:stats_interface",
        "@envoy//envoy/stats:stats_macros",
        "@envoy//source/common/protobuf:utility_lib",
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
        "@envoy//envoy/buffer:buffer_interface",
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
        "//api/envoy/config/filter/http/transformation/v2:pkg_cc_proto",
        "//source/extensions/filters/http:solo_well_known_names",
        "@envoy//envoy/buffer:buffer_interface",
        "@envoy//envoy/http:header_map_interface",
        "@envoy//source/common/common:base64_lib",
        "@envoy//source/common/common:macros",
        "@envoy//source/common/common:regex_lib",
        "@envoy//source/common/common:utility_lib",
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
        "@envoy//envoy/buffer:buffer_interface",
        "@envoy//envoy/http:header_map_interface",
        "@envoy//envoy/http:filter_interface",
        "@envoy//envoy/router:router_interface",
        "@envoy//source/common/http:header_utility_lib",
        "@envoy//source/extensions/filters/http/common:factory_base_lib",
    ],
)

envoy_cc_library(
    name = "filter_config_lib",
    hdrs = [
        "filter_config.h",
    ],
    srcs = [
        "filter_config.cc",
    ],
    repository = "@envoy",
    deps = [
        ":transformer_lib",
        ":matcher_lib",
        "//source/common/matcher:matchers_lib",
        "@envoy//envoy/buffer:buffer_interface",
        "@envoy//envoy/http:header_map_interface",
        "@envoy//envoy/http:filter_interface",
        "@envoy//envoy/router:router_interface",
        "@envoy//source/common/http:header_utility_lib",
        "@envoy//source/extensions/filters/http/common:factory_base_lib",
    ],
)

envoy_cc_library(
    name = "transformation_filter_config_lib",
    srcs = ["transformation_filter_config_factory.cc"],
    hdrs = ["transformation_filter_config_factory.h"],
    repository = "@envoy",
    deps = [
        ":transformation_filter_lib",
        "@envoy//source/extensions/filters/http/common:pass_through_filter_lib",
        "@envoy//source/extensions/filters/http/common:factory_base_lib",
    ],
)

