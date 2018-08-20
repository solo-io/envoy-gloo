licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_package",
)

envoy_package()

load("@envoy_api//bazel:api_build_system.bzl", "api_proto_library")

api_proto_library(
    name = "aws_lambda_proto",
    srcs = ["aws_lambda.proto"],
)
