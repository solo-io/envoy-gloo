REPOSITORY_LOCATIONS = dict(
    envoy = dict(
        # envoy 1.29.2 with backported ext_proc updates
        # also includes libprotobuf patch for dead stores
        commit = "4215f08044ae0bab9e74a8b248bb6480709a3a1f",
        remote = "https://github.com/solo-io/envoy-fork",
    ),
    inja = dict(
        # Includes unmerged modifications for
        # - JSON pointer syntax support
        # - Allowing escaped strings
        # - Patching dangling reference
        commit = "c0359f890c9e0d2715c1a429276da57f1403993f", # v3.4.0-patch2 patched
        remote = "https://github.com/solo-io/inja", # solo-io fork including the changes
    ),
    json = dict(
        commit = "bc889afb4c5bf1c0d8ee29ef35eaaf4c8bef8a5d",  # v3.11.2
        remote = "https://github.com/nlohmann/json",
    ),
)
