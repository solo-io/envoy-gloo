REPOSITORY_LOCATIONS = dict(
    envoy = dict(
        # envoy 1.26.7 forked with extproc changes
        # sourced from release v1.26.8-fork1
        # add fixes for async buffer limit and nlohmann json CVEs
        commit = "60f831f4e6abb1e458b2016ab36ac581ae440c65",
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
