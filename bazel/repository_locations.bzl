REPOSITORY_LOCATIONS = dict(
    # envoy-fork 1.20.0-cve0222-rc1, commit: https://github.com/solo-io/envoy-fork/commit/90a4038329611776307af420fceb56b7482e5d24
    envoy = dict(
        commit = "90a4038329611776307af420fceb56b7482e5d24",
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
