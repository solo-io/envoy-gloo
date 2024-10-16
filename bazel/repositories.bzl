load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# Use starlark for native Git.
load(
    "@bazel_tools//tools/build_defs/repo:git.bzl",
    "git_repository",
    "new_git_repository",
)
load(":repository_locations.bzl", "REPOSITORY_LOCATIONS")

# function copied from envoy: @envoy/bazel/repositories.bzl
def _repository_impl(name, **kwargs):
    # `existing_rule_keys` contains the names of repositories that have already
    # been defined in the Bazel workspace. By skipping repos with existing keys,
    # users can override dependency versions by using standard Bazel repository
    # rules in their WORKSPACE files.
    existing_rule_keys = native.existing_rules().keys()
    if name in existing_rule_keys:
        # This repository has already been defined, probably because the user
        # wants to override the version. Do nothing.
        return

    location = REPOSITORY_LOCATIONS[name]

    # Git tags are mutable. We want to depend on commit IDs instead. Give the
    # user a useful error if they accidentally specify a tag.
    if "tag" in location:
        fail(
            "Refusing to depend on Git tag %r for external dependency %r: use 'commit' instead." %
            (location["tag"], name),
        )

    if "commit" in location:
        # Git repository at given commit ID. Add a BUILD file if requested.
        if "build_file" in kwargs:
            new_git_repository(
                name = name,
                remote = location["remote"],
                commit = location["commit"],
                **kwargs
            )
        else:
            git_repository(
                name = name,
                remote = location["remote"],
                commit = location["commit"],
                **kwargs
            )
    else:  # HTTP
        # HTTP tarball at a given URL. Add a BUILD file if requested.
        if "build_file" in kwargs:
            native.new_http_archive(
                name = name,
                urls = location["urls"],
                sha256 = location["sha256"],
                strip_prefix = location["strip_prefix"],
                **kwargs
            )
        else:
            http_archive(
                name = name,
                urls = location["urls"],
                sha256 = location["sha256"],
                strip_prefix = location["strip_prefix"],
                **kwargs
            )

def envoy_gloo_dependencies():
    _repository_impl("envoy", patches=[])
    _repository_impl("json", build_file = "@envoy_gloo//bazel/external:json.BUILD")
    _repository_impl("inja", build_file = "@envoy_gloo//bazel/external:inja.BUILD")
