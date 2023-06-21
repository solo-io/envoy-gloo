REPOSITORY_LOCATIONS = dict(
    envoy = dict(
        # envoy 1.25.7
        commit = "ddb94b9d031790282f87e51f7e59a9cd61e8e55b",
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
