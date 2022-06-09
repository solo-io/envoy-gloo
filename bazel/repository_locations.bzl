REPOSITORY_LOCATIONS = dict(
    # envoy 1.21.3, commit: https://github.com/envoyproxy/envoy/releases/tag/v1.21.3
    envoy = dict(
        commit = "8c8c75fe7a2d3d2844da1de4cf66b09abf8e8227",
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
