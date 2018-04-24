licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "cache_filter_lib",
    srcs = [
        "cache_filter.cc",
    ],
    hdrs = [
        "cache_filter.h",
    ],
    repository = "@envoy",
    deps = [
        "//:cache_filter_proto_cc",
        "//include/envoy/cache:cache_interface",
        "//source/common/config:cache_well_known_names_lib",
        "@envoy//include/envoy/http:filter_interface",
        "@envoy//source/common/http:headers_lib",
        "@envoy//source/common/http:utility_lib",
    ],
)
