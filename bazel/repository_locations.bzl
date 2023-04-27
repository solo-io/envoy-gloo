REPOSITORY_LOCATIONS = dict(
    envoy = dict(
        # envoy 1.25.6 with patches applied:
        commit = "f89e0efddb8e4f8bad556608aa0e68fd5eae8d37",
        remote = "https://github.com/envoyproxy/envoy",
        # the following 2 patches are needed to support the deprecated cipher passthrough and only need to be backported onto envoy v1.25.x
        # these should be removed when moving to v1.26.x since this code exists in upstream at that point.
        patches = [
            "@envoy_gloo//bazel/foreign_cc/filter-state-matching-input-25.yaml", # https://github.com/envoyproxy/envoy/pull/25856
            "@envoy_gloo//bazel/foreign_cc/dynamic-metadata-matchingdata-25.yaml", # https://github.com/envoyproxy/envoy/pull/26311
        ],
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
