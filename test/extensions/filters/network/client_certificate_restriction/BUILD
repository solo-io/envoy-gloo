licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_binary",
    "envoy_cc_library",
    "envoy_cc_test",
    "envoy_package",
)

envoy_package()

envoy_cc_test(
    name = "client_certificate_restriction_test",
    srcs = [
        "client_certificate_restriction_test.cc",
    ],
    repository = "@envoy",
    deps = [
        "//:authorize_proto_cc",
        "//source/extensions/filters/network/client_certificate_restriction",
    ],
)
