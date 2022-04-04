REPOSITORY_LOCATIONS = dict(
    # envoy-fork 1.21.1, commit: https://github.com/envoyproxy/envoy/releases/tag/v1.21.1 with caching commit
    envoy = dict(
        commit = "584a730c320c80e44514450f71344549c01722a6",
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
