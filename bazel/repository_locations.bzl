REPOSITORY_LOCATIONS = dict(
    # envoy 1.23 pre-ga, commit: https://github.com/envoyproxy/envoy/commit/44634cc608f11bed6852c67f40902cea0d5c9554
    envoy = dict(
        commit = "44634cc608f11bed6852c67f40902cea0d5c9554",
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
