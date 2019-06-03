licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_binary",
    "envoy_cc_library",
    "envoy_package",
)
load(
    "//bazel:envoy_test.bzl",
    "envoy_gloo_cc_test",
)

envoy_package()

envoy_gloo_cc_test(
    name = "consul_connect_test",
    srcs = [
        "consul_connect_test.cc",
    ],
    repository = "@envoy",
    deps = [
        "//api/envoy/config/filter/network/consul_connect/v2:authorize_proto_cc",
        "//source/extensions/filters/network/consul_connect",
        "@envoy//test/mocks/server:server_mocks",
    ],
)
