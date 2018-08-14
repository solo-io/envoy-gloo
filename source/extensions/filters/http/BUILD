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
    name = "lambda_well_known_names_lib",
    hdrs = ["lambda_well_known_names.h"],
    repository = "@envoy",
    deps = [
        "@envoy//source/common/singleton:const_singleton",
    ],
)
