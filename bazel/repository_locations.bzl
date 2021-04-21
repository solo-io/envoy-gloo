REPOSITORY_LOCATIONS = dict(
    # bootstrap-callout branch of https://github.com/yuval-k/envoy
    # as of Dec 23rd 2020. Provides singleton fix for wasm plugin.

    # v1.17.2 release with bootstrap-extensions fix (https://github.com/envoyproxy/envoy/pull/14478)
    envoy = dict(
        commit = "94af54d11e2be9ac8c8e00663e3a08432e023042",
        remote = "https://github.com/solo-io/envoy-private",
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
