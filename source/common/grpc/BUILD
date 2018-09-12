licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "async_client_lib",
    hdrs = ["async_client_impl.h"],
    repository = "@envoy",
    deps = [
        "@envoy//include/envoy/grpc:async_client_interface",
    ],
)
