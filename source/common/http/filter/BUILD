licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "route_fault_filter_lib",
    srcs = ["route_fault_filter.cc"],
    hdrs = ["route_fault_filter.h"],
    repository = "@envoy",
    deps = [
        "//:route_fault_proto_cc",
        "//source/common/config:solo_well_known_names_lib",
        "//source/common/http:route_enabled_filter_wrapper_lib",
        "@envoy//source/common/http/filter:fault_filter_lib",
    ],
)
