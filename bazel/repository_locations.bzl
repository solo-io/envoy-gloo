REPOSITORY_LOCATIONS = dict(
    envoy = dict(
        # envoy 1.27.5 from release v1.27.5-fork1
        # add backport of https://github.com/envoyproxy/envoy/pull/33395
        commit = "98114a2ccdae0824613b7d304d6adb22ead5c9fb",
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
