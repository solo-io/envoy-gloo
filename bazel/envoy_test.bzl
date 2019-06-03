
# Copied from @envoy/bazel/envoy_test.bzl and uses public visibility for tests
# So we can target them to use our coverage.

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test_library",
)

load(
    "@envoy//bazel:envoy_internal.bzl",
    "envoy_copts",
    "envoy_select_force_libcpp",
    "envoy_linkstatic",
    "tcmalloc_external_dep",
)

# Compute the test linkopts based on various options.
def _envoy_test_linkopts():
    return select({
        "@envoy//bazel:apple": [
            # See note here: https://luajit.org/install.html
            "-pagezero_size 10000",
            "-image_base 100000000",
        ],
        "@envoy//bazel:windows_x86_64": [
            "-DEFAULTLIB:advapi32.lib",
            "-DEFAULTLIB:ws2_32.lib",
            "-WX",
        ],

        # TODO(mattklein123): It's not great that we universally link against the following libs.
        # In particular, -latomic and -lrt are not needed on all platforms. Make this more granular.
        "//conditions:default": ["-pthread", "-lrt", "-ldl"],
    }) + envoy_select_force_libcpp(["-lc++fs"], ["-lstdc++fs", "-latomic"])

# Envoy C++ test targets should be specified with this function.
def envoy_gloo_cc_test(
        name,
        srcs = [],
        data = [],
        # List of pairs (Bazel shell script target, shell script args)
        repository = "",
        external_deps = [],
        deps = [],
        tags = [],
        args = [],
        copts = [],
        shard_count = None,
        coverage = True,
        local = False,
        size = "medium"):
    test_lib_tags = []
    if coverage:
        test_lib_tags.append("coverage_test_lib")
    envoy_cc_test_library(
        name = name + "_lib_internal_only",
        srcs = srcs,
        data = data,
        external_deps = external_deps,
        deps = deps,
        repository = repository,
        tags = test_lib_tags,
        copts = copts,
    )
    native.cc_test(
        name = name,
        copts = envoy_copts(repository, test = True) + copts,
        linkopts = _envoy_test_linkopts(),
        linkstatic = envoy_linkstatic(),
        malloc = tcmalloc_external_dep(repository),
        deps = [
            ":" + name + "_lib_internal_only",
            repository + "//test:main",
        ],
        # from https://github.com/google/googletest/blob/6e1970e2376c14bf658eb88f655a054030353f9f/googlemock/src/gmock.cc#L51
        # 2 - by default, mocks act as StrictMocks.
        args = args + ["--gmock_default_mock_behavior=2"],
        tags = tags + ["coverage_test"],
        local = local,
        shard_count = shard_count,
        size = size,
    )
