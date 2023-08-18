REPOSITORY_LOCATIONS = dict(
    envoy = dict(
        # DO NOT MERGE
        # incorporate tap filter work before it merges into envoy master
        commit = "fa273d0f9884994b2f0838adc3d21ec24776b69a",
        remote = "https://github.com/ashishb-solo/envoy",
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
