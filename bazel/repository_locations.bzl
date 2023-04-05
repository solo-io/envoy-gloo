REPOSITORY_LOCATIONS = dict(
    # envoy 1.22.10, commit: https://github.com/envoyproxy/envoy/releases/tag/v1.22.6
    envoy = dict(
        commit = "ad105d154ff3e422b8e9ccc8525d46a78ee88330",
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
