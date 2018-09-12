licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_package",
)

envoy_package()

envoy_cc_test(
    name = "async_client_test",
    srcs = ["async_client_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/common/grpc:async_client_lib",
        "@envoy//test/mocks/grpc:grpc_mocks",
        "@envoy//test/mocks/tracing:tracing_mocks",
        "@envoy//test/proto:helloworld_proto_cc",
    ],
)
