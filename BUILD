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
    name = "envoy_gloo_all_filters_lib",
    repository = "@envoy",
    deps = [
        "//source/extensions/filters/http/aws_lambda:aws_lambda_filter_config_lib",
        "//source/extensions/transformers/aws_lambda:api_gateway_transformer_lib",
        "//source/extensions/filters/http/nats/streaming:nats_streaming_filter_config_lib",
        "//source/extensions/filters/http/transformation:transformation_filter_config_lib",
        "//source/extensions/filters/http/wait:config",
    ],
)

envoy_cc_binary(
    name = "envoy",
    repository = "@envoy",
    stamped = True,
    deps = [
        ":envoy_gloo_all_filters_lib",
        "@envoy//source/exe:envoy_main_entry_lib",
    ],
)
