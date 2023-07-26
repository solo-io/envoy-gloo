REPOSITORY_LOCATIONS = dict(
    envoy = dict(
        # pre-1.26.4 commit -- https://github.com/envoyproxy/envoy/commit/ac06249e76fbe2c427678c1c9d85c88675d6129e
        # using pre-release commit due to upstream build issues
        commit = "ac06249e76fbe2c427678c1c9d85c88675d6129e",
        remote = "https://github.com/envoyproxy/envoy",
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
