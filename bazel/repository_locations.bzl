REPOSITORY_LOCATIONS = dict(
    envoy = dict(
        # envoy 1.26.5 forked with extproc changes
        # sourced from release/v1.26-backportedfork
        # should go back to upstream once 1.28or wherever the associated prs are merged
        commit = "95363ffcc14059b279b6b618817aefd4bee1246b",
        remote = "https://github.com/solo-io/envoy-fork",
    ),
    inja = dict(
        # Includes unmerged modifications for
        # - JSON pointer syntax support
        # - Allowing escaped strings
        commit = "3aa95b8b58a525f86f79cb547bf937176c9cc7ff", # v3.4.0-patch1
        remote = "https://github.com/solo-io/inja", # solo-io fork including the changes
    ),
    json = dict(
        commit = "bc889afb4c5bf1c0d8ee29ef35eaaf4c8bef8a5d",  # v3.11.2
        remote = "https://github.com/nlohmann/json",
    ),
)
