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
    name = "config",
    srcs = [
        "config.cc",
    ],
    hdrs = [
        "config.h",
    ],
    repository = "@envoy",
    deps = [
        ":filter",
        "//source/extensions/filters/http:solo_well_known_names",
        "//api/envoy/config/filter/http/upstream_wait/v2:pkg_cc_proto",
        "@envoy//envoy/router:router_interface",
        "@envoy//envoy/config:typed_config_interface",
        "@envoy//source/common/protobuf:message_validator_lib",
    ],
)

envoy_cc_library(
    name = "filter",
    srcs = [
        "filter.cc",
    ],
    hdrs = [
        "filter.h",
    ],
    repository = "@envoy",
    deps = [
        "//source/extensions/filters/http:solo_well_known_names",
        "//api/envoy/config/filter/http/upstream_wait/v2:pkg_cc_proto",
        "@envoy//envoy/router:router_interface",
        "@envoy//envoy/config:typed_config_interface",
        "@envoy//source/common/protobuf:message_validator_lib",
        "@envoy//source/extensions/filters/http/common:pass_through_filter_lib",
        "@envoy//source/extensions/filters/http/common:factory_base_lib",
    ],
)
