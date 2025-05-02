REPOSITORY_LOCATIONS = dict(
    envoy = dict(
        # envoy 1.28.2 with backported ext_proc updates
        commit = "3a260838159e2d4ba6d2499e1d6bd6740e55fce5",
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
    com_googlesource_googleurl = dict(
        version = "dd4080fec0b443296c0ed0036e1e776df8813aa7",
        sha256 = "fc694942e8a7491dcc1dde1bddf48a31370a1f46fef862bc17acf07c34dc6325",
        urls = ["https://storage.googleapis.com/quiche-envoy-integration/dd4080fec0b443296c0ed0036e1e776df8813aa7.tar.gz"],
        strip_prefix = "",  # or appropriate value if needed
    ),
)
