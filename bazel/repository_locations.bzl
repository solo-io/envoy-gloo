REPOSITORY_LOCATIONS = dict(
    envoy = dict(
        # envoy fork starting from 1.25.4 with cherry-picks:
        # https://github.com/envoyproxy/envoy/pull/25856
        #   ^ add filter state matching input
        # https://github.com/envoyproxy/envoy/pull/26311
        #   ^ add dynamic metadata to MatchingData
        commit = "fa3b44a537def7ab966e2192e2693be9bede0446",
        remote = "https://github.com/solo-io/envoy-fork",
    ),
    inja = dict(
        commit = "4c0ee3a46c0bbb279b0849e5a659e52684a37a98",
        remote = "https://github.com/pantor/inja",
    ),
    json = dict(
        commit = "53c3eefa2cf790a7130fed3e13a3be35c2f2ace2",  # v3.7.0
        remote = "https://github.com/nlohmann/json",
    ),
)
