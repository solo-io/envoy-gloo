REPOSITORY_LOCATIONS = dict(
    # bootstrap-callout branch of https://github.com/yuval-k/envoy
    # as of Dec 23rd 2020. Provides singleton fix for wasm plugin.

    # v1.17.2 release
    envoy = dict(
        commit = "21fe4496ca0c7798d6a9a747fdbf1ec1071e7f4f",
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
