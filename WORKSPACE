workspace(name = "envoy_gloo")

local_repository(
    name = "envoy_build_config",
    # Relative paths are also supported.
    path = "bazel/extensions",
)

local_repository(
    name = "envoy",
    path = "/workspaces/envoy"
)

load("//bazel:repositories.bzl", "envoy_gloo_dependencies")

envoy_gloo_dependencies()

load("@envoy//bazel:api_binding.bzl", "envoy_api_binding")

envoy_api_binding()

load("@envoy//bazel:api_repositories.bzl", "envoy_api_dependencies")

envoy_api_dependencies()

load("@envoy//bazel:repositories.bzl", "envoy_dependencies")

envoy_dependencies()

load("@envoy//bazel:repositories_extra.bzl", "envoy_dependencies_extra")

# This ignore_root_user_error should be reverted if we ever get off of cloudbuild.
# Currently cloudbuild requires root access in the initial stages of setup
# https://github.com/GoogleCloudPlatform/cloud-sdk-docker/issues/214
envoy_dependencies_extra(ignore_root_user_error=True)


load("@envoy//bazel:python_dependencies.bzl", "envoy_python_dependencies")

envoy_python_dependencies()

load("@envoy//bazel:dependency_imports.bzl", "envoy_dependency_imports")

envoy_dependency_imports()
