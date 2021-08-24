REPOSITORY_LOCATIONS = dict(
    # we depend on envoy 1.19.0 with the cve patches applied on top (https://github.com/solo-io/envoy-fork/commits/release/v1.19.0-patch), since 1.19.1 would pick up changes that break our transformation filter and its unit tests
    envoy = dict(
        commit = "a837c56012d86284b8c6a1b7da87ee7c18c23d43",
        remote = "https://github.com/solo-io/envoy-fork",
    ),
    inja = dict(
        commit = "4c0ee3a46c0bbb279b0849e5a659e52684a37a98",
        remote = "https://github.com/pantor/inja",
    ),
    json = dict(
        commit = "53c3eefa2cf790a7130fed3e13a3be35c2f2ace2",  # v3.7.0
        remote = "https://github.com/nlohmann/json",
    ),
)
