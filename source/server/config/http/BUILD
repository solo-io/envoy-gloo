licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "route_fault_lib",
    srcs = ["route_fault.cc"],
    hdrs = ["route_fault.h"],
    repository = "@envoy",
    deps = [
        "//source/common/config:solo_well_known_names_lib",
        "//source/common/http:route_enabled_filter_wrapper_lib",
        "@envoy//include/envoy/registry",
        "@envoy//include/envoy/server:filter_config_interface",
        "@envoy//source/common/config:filter_json_lib",
        "@envoy//source/common/http/filter:fault_filter_lib",
    ],
)
