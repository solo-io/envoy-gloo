licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "solo_filter_utility_lib",
    srcs = ["solo_filter_utility.cc"],
    hdrs = ["solo_filter_utility.h"],
    repository = "@envoy",
    deps = [
        "@envoy//envoy/server:filter_config_interface",
        "@envoy//envoy/upstream:cluster_manager_interface",
        "@envoy//source/common/common:assert_lib",
        "@envoy//source/common/common:logger_lib",
        "@envoy//source/common/common:utility_lib",
    ],
)
