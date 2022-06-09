REPOSITORY_LOCATIONS = dict(
    # envoy 1.19.5 https://github.com/envoyproxy/envoy/releases/tag/v1.19.5
    envoy = dict(
        commit = "fccfb7d2cf663d1af898d1df076a81225d87b790",
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
