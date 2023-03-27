REPOSITORY_LOCATIONS = dict(
    # envoy 1.25.1, commit: https://github.com/envoyproxy/envoy/commit/8eca7df5099ee383f1beb37f54427957411d0936
    envoy = dict(
        commit = "1b66c3690422fb0d423411ebe70c777b9afbf2da",
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
