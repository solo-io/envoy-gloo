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
    name = "cache_well_known_names_lib",
    hdrs = ["cache_well_known_names.h"],
    repository = "@envoy",
)

envoy_cc_library(
    name = "solo_metadata_lib",
    srcs = ["solo_metadata.cc"],
    hdrs = ["solo_metadata.h"],
    external_deps = ["abseil_optional"],
    repository = "@envoy",
    deps = [
        "@envoy//source/common/protobuf",
    ],
)

envoy_cc_library(
    name = "solo_well_known_names_lib",
    hdrs = ["solo_well_known_names.h"],
    repository = "@envoy",
    deps = [
        "@envoy//source/common/singleton:const_singleton",
    ],
)

envoy_cc_library(
    name = "nats_streaming_well_known_names_lib",
    hdrs = ["nats_streaming_well_known_names.h"],
    repository = "@envoy",
)
