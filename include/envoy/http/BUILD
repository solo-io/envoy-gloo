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
    external_deps = ["abseil_optional"],
    repository = "@envoy",
    deps = [
        "@envoy//source/common/protobuf",
    ],
)
