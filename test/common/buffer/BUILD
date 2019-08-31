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
    name = "buffer_utility_test",
    srcs = ["buffer_utility_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/common/buffer:buffer_utility_lib",
        "@envoy//source/common/buffer:buffer_lib",
    ],
)
