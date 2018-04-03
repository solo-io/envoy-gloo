licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_package",
)

envoy_package()

envoy_cc_test(
    name = "solo_metadata_test",
    srcs = ["solo_metadata_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/common/config:solo_metadata_lib",
        "@envoy//source/common/config:metadata_lib",
    ],
)
