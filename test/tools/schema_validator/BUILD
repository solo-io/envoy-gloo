load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test_binary",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_test_binary(
    name = "schema_validator_tool",
    srcs = ["schema_validator.cc"],
    repository = "@envoy",
    deps = [],
)
