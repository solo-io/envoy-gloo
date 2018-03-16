licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_package",
)

envoy_package()

envoy_cc_test(
    name = "transformation_filter_test",
    srcs = [
        "functional_transformation_filter_test.cc",
        "transformation_filter_test.cc",
    ],
    repository = "@envoy",
    deps = [
        "//:filter_lib",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/server:server_mocks",
        "@envoy//test/mocks/upstream:upstream_mocks",
        "@envoy//test/test_common:utility_lib",
    ],
)

envoy_cc_test(
    name = "transformer_test",
    srcs = ["transformer_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/common/http/filter:transformer_lib",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/server:server_mocks",
        "@envoy//test/mocks/upstream:upstream_mocks",
        "@envoy//test/test_common:utility_lib",
    ],
)
