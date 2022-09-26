REPOSITORY_LOCATIONS = dict(
    # envoy 1.23.1, commit: https://github.com/envoyproxy/envoy/commit/1c86bac121ae73cefcba64ec0a863707b6cb8158
    envoy = dict(
        commit = "1c86bac121ae73cefcba64ec0a863707b6cb8158",
        remote = "https://github.com/envoyproxy/envoy",
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
