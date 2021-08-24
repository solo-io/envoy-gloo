REPOSITORY_LOCATIONS = dict(
    # v1.17.4 release with bootstrap extension branch. Provides singleton fix for wasm plugin. 
    # (https://github.com/envoyproxy/envoy/commit/0de60452be1fa329c16076916b6bfe1f672aeed4) 
    envoy = dict(
        commit = "7327c8a932716ffd57dd343ae812b3d300fe6ee1",
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
