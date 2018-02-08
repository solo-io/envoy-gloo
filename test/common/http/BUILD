licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_package",
)

envoy_package()

envoy_cc_test(
    name = "functional_stream_decoder_base_test",
    srcs = ["functional_stream_decoder_base_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/common/http:functional_stream_decoder_base_lib",
        "@envoy//include/envoy/event:dispatcher_interface",
        "@envoy//source/common/http:header_map_lib",
        "@envoy//source/common/stats:stats_lib",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/server:server_mocks",
        "@envoy//test/mocks/upstream:upstream_mocks",
    ],
)
