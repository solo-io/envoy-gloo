REPOSITORY_LOCATIONS = dict(
    # envoy-fork 1.21.5-patch1
    # This release represents upstream's v1.21.5 with the April 4, 2023 CVE patches applied
    # The envoy-gloo release nomenclature will continue with the -patchX incrementing
    # from the previous envoy-gloo release
    envoy = dict(
        commit = "6658a0d3a0bbc1072fe4e9d88003cf2530813893",
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
