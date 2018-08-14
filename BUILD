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

api_proto_library(
    name = "authorize_proto",
    srcs = ["authorize.proto"],
)

api_proto_library(
    name = "functional_base_proto",
    srcs = ["functional_base.proto"],
)

envoy_cc_binary(
    name = "envoy",
    repository = "@envoy",
    deps = [
        "//source/extensions/filters/http/aws:lambda_filter_config_lib",
        "//source/extensions/filters/network/consul_connect:config",
        "@envoy//source/exe:envoy_main_entry_lib",
    ],
)
