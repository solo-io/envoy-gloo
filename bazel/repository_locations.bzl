REPOSITORY_LOCATIONS = dict(
    # can't have more than one comment between envoy line and commit line in 
    # order to accommodate `check_extensions_build_config.sh`
    envoy = dict(
        # envoy v1.31.2++ ci and checksums
        commit = "d599cb3320b890a6caef865b22d197a2314a5ac4",
        remote = "https://github.com/envoyproxy/envoy",
    ),
    inja = dict(
        # Includes unmerged modifications for
        # - JSON pointer syntax support
        # - Allowing escaped strings
        # - Patching dangling reference
        commit = "1ee6ec1b89e73f1257b27242f394979f6de85e77", # v3.4.0-patch3
        remote = "https://github.com/solo-io/inja", # solo-io fork including the changes
    ),
    json = dict(
        commit = "bc889afb4c5bf1c0d8ee29ef35eaaf4c8bef8a5d",  # v3.11.2
        remote = "https://github.com/nlohmann/json",
    ),
)
