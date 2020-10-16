REPOSITORY_LOCATIONS = dict(
    # envoy master-v1.17.0-pre
    envoy = dict(
        commit = "2097fe908f2abb718757dbd4087d793c861d7c5a",
        remote = "https://github.com/envoyproxy/envoy",
    ),
    inja = dict(
        # commit = "4c0ee3a46c0bbb279b0849e5a659e52684a37a98",
        # remote = "https://github.com/pantor/inja",
        sha256 = "d553741d010c890290f19a4fb182bb82eadbc8d4cfe19b73dcce1426f26d19f0",
        strip_prefix = "inja-4c0ee3a46c0bbb279b0849e5a659e52684a37a98",
        url = "https://github.com/pantor/inja/archive/4c0ee3a46c0bbb279b0849e5a659e52684a37a98.tar.gz",
    ),
    json = dict(
        # commit = "53c3eefa2cf790a7130fed3e13a3be35c2f2ace2",  # v3.7.0
        # remote = "https://github.com/nlohmann/json",
        sha256 = "4b9b678e17f7477350671f2f0cf32e1151bbecbf584eb08906d9aa6d66fbef12",
        strip_prefix = "json-53c3eefa2cf790a7130fed3e13a3be35c2f2ace2",
        url = "https://github.com/nlohmann/json/archive/53c3eefa2cf790a7130fed3e13a3be35c2f2ace2.tar.gz",
    ),
)
