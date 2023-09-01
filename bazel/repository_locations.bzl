REPOSITORY_LOCATIONS = dict(
    envoy = dict(
        commit = "06f7930b4152e9f37a315fec8b62bdb88aff945c",
        remote = "https://github.com/solo-io/envoy-fork",
    ),
    # envoy = dict(
    #     # envoy 1.26.4
    #     commit = "cfa32deca25ac57c2bbecdad72807a9b13493fc1",
    #     remote = "https://github.com/envoyproxy/envoy",
    # ),
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
