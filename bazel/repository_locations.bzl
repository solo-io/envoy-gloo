REPOSITORY_LOCATIONS = dict(
    envoy = dict(
        # envoy 1.26.3
        commit = "ea9d25e93cef74b023c95ca1a3f79449cdf7fa9a",
        remote = "https://github.com/envoyproxy/envoy",
    ),
    inja = dict(
        commit = "5a18986825fc7e5d2916ff345c4369e4b6ea7122", # v3.4 + JSON Pointers
        remote = "https://github.com/solo-io/inja", # solo-io fork including the changes
    ),
    json = dict(
        commit = "bc889afb4c5bf1c0d8ee29ef35eaaf4c8bef8a5d",  # v3.11.2
        remote = "https://github.com/nlohmann/json",
    ),
)
