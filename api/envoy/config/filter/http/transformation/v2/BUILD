licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_package",
)

envoy_package()

load("@envoy_api//bazel:api_build_system.bzl", "api_proto_package")

api_proto_package(
    deps = [
        "@envoy_api//envoy/api/v2/route:pkg",
        "@envoy_api//envoy/type/matcher:pkg",
        "@envoy_api//envoy/type:pkg",
    ],
    visibility = ["//visibility:public"],)
