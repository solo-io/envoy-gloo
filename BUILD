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
    ],
)

envoy_cc_library(
    name = "envoy_all_clusters_lib",
    repository = "@envoy",
    deps = [
        "@envoy//source/extensions/clusters/aggregate:cluster",
        "@envoy//source/extensions/clusters/dynamic_forward_proxy:cluster",
        "@envoy//source/extensions/clusters/eds:eds_lib",
        "@envoy//source/extensions/clusters/logical_dns:logical_dns_cluster_lib",
        "@envoy//source/extensions/clusters/original_dst:original_dst_cluster_lib",
        "@envoy//source/extensions/clusters/static:static_cluster_lib",
        "@envoy//source/extensions/clusters/strict_dns:strict_dns_cluster_lib",
    ],
)

envoy_cc_binary(
    name = "envoy",
    repository = "@envoy",
    stamped = True,
    deps = [
        ":envoy_gloo_all_filters_lib",
        ":envoy_all_clusters_lib",
        "@envoy//source/exe:envoy_main_entry_lib",
    ],
)
