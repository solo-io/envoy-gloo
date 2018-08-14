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
    ],
)

envoy_cc_test(
    name = "transformer_test",
    srcs = ["transformer_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/extensions/filters/http/transformation:transformer_lib",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/server:server_mocks",
        "@envoy//test/mocks/upstream:upstream_mocks",
    ],
)

envoy_cc_test(
    name = "body_header_transformer_test",
    srcs = ["body_header_transformer_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/extensions/filters/http/transformation:body_header_transformer_lib",
        "@envoy//test/test_common:utility_lib",
    ],
)

envoy_cc_test(
    name = "transformation_filter_integration_test",
    srcs = ["transformation_filter_integration_test.cc"],
    repository = "@envoy",
    deps = [
        "//:filter_lib",
        "@envoy//test/integration:http_integration_lib",
        "@envoy//test/integration:integration_lib",
    ],
)

envoy_cc_test(
    name = "transformation_filter_route_integration_test",
    srcs = ["transformation_filter_route_integration_test.cc"],
    repository = "@envoy",
    deps = [
        "//:filter_lib",
        "@envoy//test/integration:http_integration_lib",
        "@envoy//test/integration:integration_lib",
    ],
)
