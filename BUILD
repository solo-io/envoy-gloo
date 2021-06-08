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
        "//source/extensions/filters/http/nats/streaming:nats_streaming_filter_config_lib",
        "//source/extensions/filters/http/transformation:transformation_filter_config_lib",
    ],
)

envoy_cc_library(
    name = "istio_proxy_all_filters_lib",
    repository = "@envoy",
    deps = [
        "@io_istio_proxy//extensions/access_log_policy:access_log_policy_lib",
        "@io_istio_proxy//extensions/attributegen:attributegen_plugin",
        "@io_istio_proxy//extensions/metadata_exchange:metadata_exchange_lib",
        "@io_istio_proxy//extensions/stackdriver:stackdriver_plugin",
        "@io_istio_proxy//extensions/stats:stats_plugin",
        "@io_istio_proxy//src/envoy/extensions/wasm:wasm_lib",
        "@io_istio_proxy//src/envoy/http/alpn:config_lib",
        "@io_istio_proxy//src/envoy/http/authn:filter_lib",
        "@io_istio_proxy//src/envoy/tcp/forward_downstream_sni:config_lib",
        "@io_istio_proxy//src/envoy/tcp/metadata_exchange:config_lib",
        "@io_istio_proxy//src/envoy/tcp/sni_verifier:config_lib",
        "@io_istio_proxy//src/envoy/tcp/tcp_cluster_rewrite:config_lib",
    ],
)

envoy_cc_binary(
    name = "envoy",
    repository = "@envoy",
    stamped = True,
    deps = [
        ":envoy_gloo_all_filters_lib",
        ":istio_proxy_all_filters_lib",
        "@envoy//source/exe:envoy_main_entry_lib",
    ],
)
