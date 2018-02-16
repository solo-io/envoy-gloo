licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "metadata_accessor_interface",
    hdrs = ["metadata_accessor.h"],
    repository = "@envoy",
    deps = [
        "@envoy//include/envoy/common:optional",
        "@envoy//source/common/protobuf",
        ],
)
