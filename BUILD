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
    name = "cache_filter_proto",
    srcs = ["cache_filter.proto"],
)

api_proto_library(
    name = "protocol_proto",
    srcs = ["protocol.proto"],
)

api_proto_library(
    name = "transformation_filter_proto",
    srcs = ["transformation_filter.proto"],
)

envoy_cc_binary(
    name = "envoy",
    repository = "@envoy",
    stamped = True,
    deps = [
        "//source/extensions/filters/http/aws_lambda:aws_lambda_filter_config_lib",
        "//source/extensions/filters/http/cache:cache_filter_config_lib",
        "//source/extensions/filters/http/nats/streaming:nats_streaming_filter_config_lib",
        "//source/extensions/filters/http/transformation:transformation_filter_config_lib",
        "//source/extensions/filters/network/consul_connect:config",
        "@envoy//source/exe:envoy_main_entry_lib",
    ],
)
