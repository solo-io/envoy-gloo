REPOSITORY_LOCATIONS = dict(
    # envoy-fork 1.21.7 ??? using to test CI
    envoy = dict(
        commit = "9e7b8eb6694385cf831f94b497afc3c6275cc5cd",
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
