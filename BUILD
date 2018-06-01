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

envoy_cc_binary(
    name = "envoy",
    repository = "@envoy",
    deps = [
        ":client_certificate_restriction_config",
        "@envoy//source/exe:envoy_main_entry_lib",
    ],
)

envoy_cc_library(
    name = "client_certificate_restriction_lib",
    srcs = ["client_certificate_restriction.cc"],
    hdrs = ["client_certificate_restriction.h"],
    repository = "@envoy",
    deps = [
        #        "@envoy//include/envoy/buffer:buffer_interface",
        #        "@envoy//include/envoy/network:connection_interface",
        "@envoy//include/envoy/network:filter_interface",
        "@envoy//source/common/common:assert_lib",
        "@envoy//source/common/common:logger_lib",
    ],
)

envoy_cc_library(
    name = "client_certificate_restriction_config",
    srcs = ["client_certificate_restriction_config.cc"],
    repository = "@envoy",
    deps = [
        ":client_certificate_restriction_lib",
        "@envoy//include/envoy/network:filter_interface",
        "@envoy//include/envoy/registry",
        "@envoy//include/envoy/server:filter_config_interface",
    ],
)
