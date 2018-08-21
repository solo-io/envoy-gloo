workspace(name="envoy_gloo")

# Use skylark for native Git.
load('@bazel_tools//tools/build_defs/repo:git.bzl', 'git_repository')

ENVOY_SHA = "a51d86aeb5a15f50472ee87a7b54c797be2c15fb"  # 2018-08-10

http_archive(
    name = "envoy",
    strip_prefix = "envoy-" + ENVOY_SHA,
    url = "https://github.com/envoyproxy/envoy/archive/" + ENVOY_SHA + ".zip",
)

JSON_SHA = "d2dd27dc3b8472dbaa7d66f83619b3ebcd9185fe" # v3.1.2

new_http_archive(
    name = "json",
    strip_prefix = "json-" + JSON_SHA + "/single_include/nlohmann",
    url = "https://github.com/nlohmann/json/archive/" + JSON_SHA + ".zip",
    build_file_content = """
cc_library(
    name = "json-lib",
    hdrs = ["json.hpp"],
    visibility = ["//visibility:public"],
)
    """
)

INJA_SHA = "74ad4281edd4ceca658888602af74bf2050107f0"

new_http_archive(
    name = "inja",
    strip_prefix = "inja-" + INJA_SHA + "/src",
    url = "https://github.com/pantor/inja/archive/" + INJA_SHA + ".zip",
    build_file_content = """
cc_library(
    name = "inja-lib",
    hdrs = ["inja.hpp"],
    visibility = ["//visibility:public"],
)
    """
)

load("@envoy//bazel:repositories.bzl", "envoy_dependencies")
load("@envoy//bazel:cc_configure.bzl", "cc_configure")

envoy_dependencies()

cc_configure()

load("@envoy_api//bazel:repositories.bzl", "api_dependencies")
api_dependencies()

load("@io_bazel_rules_go//go:def.bzl", "go_rules_dependencies", "go_register_toolchains")
load("@com_lyft_protoc_gen_validate//bazel:go_proto_library.bzl", "go_proto_repositories")
go_proto_repositories(shared=0)
go_rules_dependencies()
go_register_toolchains()
load("@io_bazel_rules_go//proto:def.bzl", "proto_register_toolchains")
proto_register_toolchains()
