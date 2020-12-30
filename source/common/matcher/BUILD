licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_binary",
    "envoy_cc_library",
    "envoy_cc_test",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "matchers_lib",
    srcs = ["solo_matcher.cc"],
    hdrs = ["solo_matcher.h"],
    repository = "@envoy",
    external_deps = ["abseil_optional"],
    deps = [
        "@envoy//source/common/router:config_lib",
        "@envoy//source/common/http:header_utility_lib",
        "@envoy_api//envoy/api/v2/route:pkg_cc_proto",
        "@envoy_api//envoy/config/route/v3:pkg_cc_proto",
    ],
)

