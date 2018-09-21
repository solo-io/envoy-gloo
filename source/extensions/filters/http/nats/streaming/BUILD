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
    name = "nats_streaming_filter_config",
    hdrs = [
        "nats_streaming_filter_config.h",
    ],
    repository = "@envoy",
    deps = [
        "//api/envoy/config/filter/http/nats/streaming/v2:nats_streaming_cc",
    ],
)

envoy_cc_library(
    name = "nats_streaming_filter_lib",
    srcs = [
        "nats_streaming_filter.cc",
    ],
    hdrs = [
        "nats_streaming_filter.h",
    ],
    repository = "@envoy",
    deps = [
        ":nats_streaming_filter_config",
        ":nats_streaming_route_specific_filter_config",
        "//include/envoy/nats/streaming:client_interface",
        "//source/common/http:solo_filter_utility_lib",
        "//source/extensions/filters/http:solo_well_known_names",
    ],
)

envoy_cc_library(
    name = "nats_streaming_filter_config_lib",
    srcs = ["nats_streaming_filter_config_factory.cc"],
    hdrs = ["nats_streaming_filter_config_factory.h"],
    repository = "@envoy",
    deps = [
        ":nats_streaming_filter_lib",
        "//source/common/nats:codec_lib",
        "//source/common/nats/streaming:client_pool_lib",
        "//source/common/tcp:conn_pool_lib",
        "//source/extensions/filters/http:solo_well_known_names",
        "@envoy//source/extensions/filters/http/common:factory_base_lib",
    ],
)

envoy_cc_library(
    name = "nats_streaming_route_specific_filter_config",
    srcs = ["nats_streaming_route_specific_filter_config.cc"],
    hdrs = ["nats_streaming_route_specific_filter_config.h"],
    repository = "@envoy",
    deps = [
        "//api/envoy/config/filter/http/nats/streaming/v2:nats_streaming_cc",
        "@envoy//include/envoy/registry",
        "@envoy//include/envoy/router:router_interface",
    ],
)
