licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_binary",
    "envoy_cc_library",
    "envoy_cc_test",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "aws_authenticator_lib",
    srcs = ["aws_authenticator.cc"],
    hdrs = ["aws_authenticator.h"],
    external_deps = ["ssl"],
    repository = "@envoy",
    deps = [
        "@envoy//include/envoy/buffer:buffer_interface",
        "@envoy//include/envoy/http:header_map_interface",
        "@envoy//source/common/common:assert_lib",
        "@envoy//source/common/common:empty_string",
        "@envoy//source/common/common:hex_lib",
        "@envoy//source/common/http:headers_lib",
        "@envoy//source/common/http:utility_lib",
    ],
)

envoy_cc_library(
    name = "aws_lambda_filter_lib",
    srcs = [
        "aws_lambda_filter.cc",
    ],
    hdrs = [
        "aws_lambda_filter.h",
    ],
    repository = "@envoy",
    deps = [
        ":aws_authenticator_lib",
        ":config_lib",
        "//api/envoy/config/filter/http/aws_lambda/v2:pkg_cc_proto",
        "//source/common/http:solo_filter_utility_lib",
        "//source/extensions/filters/http:solo_well_known_names",
        "@envoy//source/common/http:utility_lib",
    ],
)

envoy_cc_library(
    name = "config_lib",
    srcs = [
        "config.cc",
    ],
    hdrs = [
        "config.h",
        "stats.h",
    ],
    repository = "@envoy",
    deps = [
        ":aws_authenticator_lib",
        "//api/envoy/config/filter/http/aws_lambda/v2:pkg_cc_proto",
        "//source/common/http:solo_filter_utility_lib",
        "//source/extensions/filters/http:solo_well_known_names",
        "@envoy//source/common/http:utility_lib",
        "@envoy//source/extensions/common/aws:credentials_provider_impl_lib",
        "@envoy//source/extensions/common/aws:credentials_provider_interface",
        "@envoy//source/extensions/common/aws:utility_lib",
    ],
)

envoy_cc_library(
    name = "aws_lambda_filter_config_lib",
    srcs = ["aws_lambda_filter_config_factory.cc"],
    hdrs = ["aws_lambda_filter_config_factory.h"],
    repository = "@envoy",
    deps = [
        ":aws_lambda_filter_lib",
        "@envoy//include/envoy/registry",
        "@envoy//include/envoy/server:filter_config_interface",
        "@envoy//source/extensions/filters/http/common:factory_base_lib",
    ],
)

envoy_cc_library(
    name = "sts_credentials_provider_lib",
    srcs = ["sts_credentials_provider.cc"],
    hdrs = ["sts_credentials_provider.h", "stats.h"],
    external_deps = ["curl"],
    repository = "@envoy",
    deps = [
        "//api/envoy/config/filter/http/aws_lambda/v2:pkg_cc_proto",
        "@envoy//source/common/common:assert_lib",
        "@envoy//source/common/http:headers_lib",
        "@envoy//source/common/http:utility_lib",
        "@envoy//include/envoy/server:transport_socket_config_interface",
        "@envoy//source/extensions/common/aws:credentials_provider_interface",
    ],
)
