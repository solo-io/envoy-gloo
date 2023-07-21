REPOSITORY_LOCATIONS = dict(
    envoy = dict(
        # envoy 1.26.3
        commit = "ea9d25e93cef74b023c95ca1a3f79449cdf7fa9a",
        remote = "https://github.com/envoyproxy/envoy",
    ),
    inja = dict(
        # Includes unmerged modifications for
        # - JSON pointer syntax support
        # - Allowing escaped strings
        commit = "2c441a3ca0b66bdab61f3f9044fb075ae6cacea4", # v3.4.0-patch2
        remote = "https://github.com/solo-io/inja", # solo-io fork including the changes
    ),
    json = dict(
        commit = "bc889afb4c5bf1c0d8ee29ef35eaaf4c8bef8a5d",  # v3.11.2
        remote = "https://github.com/nlohmann/json",
    ),
)
