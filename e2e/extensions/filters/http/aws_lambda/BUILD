licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_package",
    "envoy_sh_test",
)

envoy_package()

load("@envoy_api//bazel:api_build_system.bzl", "api_cc_py_proto_library")

sh_test(
    name = "e2e2e_test",
    srcs = ["e2e2e_test.sh"],
    data = [
        "//:envoy",
        "//e2e/extensions/filters/http/aws_lambda:create_config.sh",
        "//e2e/extensions/filters/http/aws_lambda:create_config_env.sh",
    ],
)
