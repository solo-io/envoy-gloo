REPOSITORY_LOCATIONS = dict(
    # envoy-fork 1.21.7 ??? using to test CI
    envoy = dict(
        commit = "696db3f1cbdac9752b85d6a1a90547d11cc52231",
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
