licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test_binary",
    "envoy_package",
)
load(
    "//bazel:envoy_test.bzl",
    "envoy_gloo_cc_test",
)
load("@envoy_api//bazel:api_build_system.bzl", "api_proto_package")

envoy_package()

envoy_gloo_cc_test(
    name = "inja_transformer_test",
    srcs = ["inja_transformer_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/extensions/filters/http/transformation:inja_transformer_lib",
        "@envoy//source/common/common:random_generator_lib",
        "@envoy//source/common/common:base64_lib",
        "@envoy//test/test_common:environment_lib",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/server:server_mocks",
        "@envoy//test/mocks/upstream:upstream_mocks",
    ],
)

envoy_gloo_cc_test(
    name = "inja_transformer_replace_test",
    srcs = ["inja_transformer_replace_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/extensions/filters/http/transformation:inja_transformer_lib",
        "@envoy//source/common/common:random_generator_lib",
        "@envoy//source/common/common:base64_lib",
        "@envoy//test/test_common:environment_lib",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/server:server_mocks",
        "@envoy//test/mocks/upstream:upstream_mocks",
    ],
)

envoy_cc_test_binary(
    name = "inja_transformer_speed_test",
    srcs = ["inja_transformer_speed_test.cc"],
    external_deps = [
        "benchmark",
    ],
    repository = "@envoy",
    deps = [
        "//source/extensions/filters/http/transformation:inja_transformer_lib",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/server:server_mocks",
        "@envoy//test/mocks/upstream:upstream_mocks",
    ],
)

envoy_gloo_cc_test(
    name = "transformation_filter_test",
    srcs = ["transformation_filter_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/extensions/filters/http:solo_well_known_names",
        "//source/extensions/filters/http/transformation:transformation_filter_lib",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/server:server_mocks",
        "@envoy//test/mocks/upstream:upstream_mocks",
        "@envoy//test/common/common:logger_test",
        "@envoy//source/common/http/matching:inputs_lib",
    ],
)

envoy_gloo_cc_test(
    name = "matcher_test",
    srcs = ["matcher_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/extensions/filters/http/transformation:matcher_lib",
        "@envoy//test/mocks/server:server_mocks",
        "@envoy//test/mocks/stream_info:stream_info_mocks",
        "@envoy//source/common/http/matching:inputs_lib",
    ],
)

envoy_gloo_cc_test(
    name = "body_header_transformer_test",
    srcs = ["body_header_transformer_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/extensions/filters/http/transformation:body_header_transformer_lib",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/server:server_mocks",
        "@envoy//test/test_common:utility_lib",
    ],
)

envoy_gloo_cc_test(
    name = "transformation_filter_config_test",
    repository = "@envoy",
    srcs = ["fake_transformer.h", "transformation_filter_config_test.cc"],
    deps = [
        "@envoy//test/test_common:utility_lib",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/server:server_mocks",
        "//source/extensions/filters/http/transformation:transformation_filter_lib",
        ":pkg_cc_proto",
    ]
)

api_proto_package(
    visibility = ["//visibility:public"],
)
