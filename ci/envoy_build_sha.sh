# Source: https://github.com/envoyproxy/envoy/blob/arm64_release/ci/envoy_build_sha.sh
ENVOY_BUILD_SHA=$(grep envoyproxy/envoy-build-ubuntu $(dirname $0)/../.bazelrc | sed -e 's#.*envoyproxy/envoy-build-ubuntu:\(.*\)#\1#' | uniq)
[[ $(wc -l <<< "${ENVOY_BUILD_SHA}" | awk '{$1=$1};1') == 1 ]] || (echo ".bazelrc envoyproxy/envoy-build-ubuntu hashes are inconsistent!" && exit 1)
