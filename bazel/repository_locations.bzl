REPOSITORY_LOCATIONS = dict(
    # envoy 1.25.2, commit: https://github.com/envoyproxy/envoy/commit/afa98867c807dee0d833da701ba3ab0c9ace9ada
    envoy = dict(
        commit = "afa98867c807dee0d833da701ba3ab0c9ace9ada",
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
