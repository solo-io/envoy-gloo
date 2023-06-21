REPOSITORY_LOCATIONS = dict(
    envoy = dict(
        # envoy 1.26.2
        commit = "4aa28dcbfdaf17b754ee6d4ca4f1fdf6f84c99dc",
        remote = "https://github.com/envoyproxy/envoy",
    ),
    inja = dict(
        commit = "5a18986825fc7e5d2916ff345c4369e4b6ea7122", # v3.4 + JSON Pointers
        remote = "https://github.com/pantor/inja",
    ),
    json = dict(
        commit = "bc889afb4c5bf1c0d8ee29ef35eaaf4c8bef8a5d",  # v3.11.2
        remote = "https://github.com/nlohmann/json",
    ),
)
