load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test_binary",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_test_binary(
    name = "config_load_check_tool",
    srcs = ["config_load_check.cc"],
    repository = "@envoy",
    deps = [],
)
