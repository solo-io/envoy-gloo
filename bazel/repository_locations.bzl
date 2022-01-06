REPOSITORY_LOCATIONS = dict(
    # envoy 1.20.0, commit: https://github.com/envoyproxy/envoy/commit/96701cb24611b0f3aac1cc0dd8bf8589fbdf8e9e
    envoy = dict(
        commit = "4bcda9e6e5fea8d8a7b92341400ed849de50e1bd",
        remote = "https://github.com/EItanya/envoy",
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
