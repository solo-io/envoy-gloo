REPOSITORY_LOCATIONS = dict(
    # envoy 1.24.7, commit: https://github.com/envoyproxy/envoy/commit/b32b2fa063a9b4037c59839ed5cd39099e2cc20f
    envoy = dict(
        commit = "b32b2fa063a9b4037c59839ed5cd39099e2cc20f",
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
