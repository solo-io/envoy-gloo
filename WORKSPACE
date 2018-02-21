workspace(name="transformation_filter")

ENVOY_SHA = "f79a62b7cc9ca55d20104379ee0576617630cdaa"  # Feb 15, 2018 ( test: fix nit after #2591 (#2601) )

http_archive(
    name = "envoy",
    strip_prefix = "envoy-" + ENVOY_SHA,
    url = "https://github.com/envoyproxy/envoy/archive/" + ENVOY_SHA + ".zip",
)

ENVOY_COMMON_SHA = "0fefba1a893e9d8e24700b0c5919f301a3d10249"  # Feb 19, 2018 (let the child filter  retrieve the function.)

# load solo common
git_repository(
    name = "envoy_common",
    remote = "git@github.com:solo-io/envoy-common",
    commit = ENVOY_COMMON_SHA,
)


JSON_SHA = "c8ea63a31bbcf652d61490b0ccd86771538f8c6b"

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