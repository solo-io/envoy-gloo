REPOSITORY_LOCATIONS = dict(
    # envoy 1.24.0, commit: https://github.com/envoyproxy/envoy/commit/15baf56003f33a07e0ab44f82f75a660040db438
    envoy = dict(
        commit = "9184b84cd0dcb3a6c57eb44b177d91c70e1a0901",
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
