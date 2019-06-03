licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_package",
)
load(
    "//bazel:envoy_test.bzl",
    "envoy_gloo_cc_test",
)

envoy_package()

envoy_gloo_cc_test(
    name = "solo_filter_utility_test",
    srcs = ["solo_filter_utility_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/common/http:solo_filter_utility_lib",
        "@envoy//test/mocks/http:http_mocks",
    ],
)
