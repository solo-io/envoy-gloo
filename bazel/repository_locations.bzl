REPOSITORY_LOCATIONS = dict(
    # envoy 1.22.10, commit: https://github.com/envoyproxy/envoy/releases/tag/v1.22.10
    envoy = dict(
        commit = "ea99b9424288108951062d80f31bbb020ec32b38",
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
