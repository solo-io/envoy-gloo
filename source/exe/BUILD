licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_binary",
    "envoy_cc_library",
    "envoy_cc_test",
    "envoy_package",
)

envoy_package()

alias(
    name = "envoy",
    actual = ":envoy-static",
)

envoy_cc_binary(
    name = "envoy-static",
    repository = "@envoy",
    stamped = True,
    deps = [
        "//:envoy_gloo_all_filters_lib",
        "@envoy//source/exe:envoy_main_entry_lib",
    ],
)
