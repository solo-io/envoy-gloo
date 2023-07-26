REPOSITORY_LOCATIONS = dict(
    envoy = dict(
        # pre-1.26.4 commit -- https://github.com/envoyproxy/envoy/commit/e0bca625f98a0270a78e7c1aa787ca4241eb7d60
        # using pre-release commit due to upstream build issues https://github.com/envoyproxy/envoy/issues/28639
        commit = "e0bca625f98a0270a78e7c1aa787ca4241eb7d60",
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
