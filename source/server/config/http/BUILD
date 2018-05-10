licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "cache_filter_config_lib",
    srcs = ["cache_filter_config_factory.cc"],
    hdrs = ["cache_filter_config_factory.h"],
    repository = "@envoy",
    deps = [
        "//source/common/cache/combinator:cache_lib",
        "//source/common/cache/grpc:cache_lib",
        "//source/common/cache/in_memory:cache_lib",
        "//source/common/cache/redis:cache_lib",
        "//source/common/config:cache_well_known_names_lib",
        "//source/common/http/filter:cache_filter_lib",
        "@envoy//source/common/config:filter_json_lib",
        "@envoy//source/common/config:well_known_names",
        "@envoy//source/extensions/filters/http/common:empty_http_filter_config_lib",
        "@envoy//source/extensions/filters/network/redis_proxy:command_splitter_lib",
        "@envoy//source/extensions/filters/network/redis_proxy:conn_pool_lib",
    ],
)
