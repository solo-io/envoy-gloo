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
    name = "solo_well_known_names",
    hdrs = ["solo_well_known_names.h"],
    repository = "@envoy",
    deps = [
        "@envoy//source/common/singleton:const_singleton",
    ],
)
