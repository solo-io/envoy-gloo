REPOSITORY_LOCATIONS = dict(
    envoy = dict(
        # envoy 1.25.6 with patches applied:
        # https://github.com/envoyproxy/envoy/pull/25856
        #   ^ add filter state matching input
        #     https://github.com/envoyproxy/envoy/commit/f52d559e0479824b9c964e4c028fa373bcb9b767
        # https://github.com/envoyproxy/envoy/pull/26311
        #   ^ add dynamic metadata to MatchingData
        #     https://github.com/envoyproxy/envoy/commit/1b66c3690422fb0d423411ebe70c777b9afbf2da
        commit = "f89e0efddb8e4f8bad556608aa0e68fd5eae8d37",
        remote = "https://github.com/envoyproxy/envoy",
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
