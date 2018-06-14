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
    name = "client_certificate_restriction",
    srcs = ["client_certificate_restriction.cc"],
    hdrs = ["client_certificate_restriction.h"],
    repository = "@envoy",
    deps = [
        "//:authorize_proto_cc",
        "//api/envoy/config/filter/network/client_certificate_restriction/v2:client_certificate_restriction_proto_cc",
        "@envoy//include/envoy/network:filter_interface",
        "@envoy//include/envoy/upstream:cluster_manager_interface",
        "@envoy//source/common/common:assert_lib",
        "@envoy//source/common/common:logger_lib",
        "@envoy//source/common/ssl:ssl_socket_lib",
    ],
)

envoy_cc_library(
    name = "config",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    repository = "@envoy",
    deps = [
        ":client_certificate_restriction",
        "//source/extensions/filters/network:client_certificate_restriction_well_known_names",
        "@envoy//include/envoy/network:filter_interface",
        "@envoy//include/envoy/registry",
        "@envoy//include/envoy/server:filter_config_interface",
        "@envoy//source/extensions/filters/network/common:factory_base_lib",
    ],
)
