REPOSITORY_LOCATIONS = dict(
    # v1.17.2 release with bootstrap extension branch. Provides singleton fix for wasm plugin. 
    # (https://github.com/envoyproxy/envoy/commit/e899c7b0cda0a4a4050afff07670a5a7dbbd0e5e) 
    envoy = dict(
        commit = "c54077894733bc4f056f74d166c50c6ccdda59fc",
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
