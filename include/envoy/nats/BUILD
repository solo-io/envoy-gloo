licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "codec_interface",
    hdrs = ["codec.h"],
    repository = "@envoy",
    deps = ["@envoy//envoy/buffer:buffer_interface"],
)

envoy_cc_library(
    name = "token_generator_interface",
    hdrs = ["token_generator.h"],
    repository = "@envoy",
)
