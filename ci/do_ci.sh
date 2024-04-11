#!/bin/bash
set -e

if [ -n "$ENVOY_DOCKER_BUILD_DIR" ]; then
  rm -rf "$ENVOY_DOCKER_BUILD_DIR/envoy/x64/bin/"
else
  rm -rf "/build/envoy/x64/bin/"
fi

bazel fetch //source/exe:envoy-static

SOURCE_DIR="$(bazel info workspace)"

"$SOURCE_DIR/ci/verify_posture.sh" verify

export UPSTREAM_ENVOY_SRCDIR=$(bazel info output_base)/external/envoy

cp -f "$UPSTREAM_ENVOY_SRCDIR/.bazelrc"                    "$SOURCE_DIR/upstream.bazelrc"
cp -f "$UPSTREAM_ENVOY_SRCDIR/.bazelversion"               "$SOURCE_DIR/.bazelversion"
cp -f "$UPSTREAM_ENVOY_SRCDIR/ci/WORKSPACE.filter.example" "$SOURCE_DIR/ci/"
cp -f "$UPSTREAM_ENVOY_SRCDIR/VERSION.txt"                 "$SOURCE_DIR/VERSION.txt"


if [ -f "$UPSTREAM_ENVOY_SRCDIR/bazel/setup_clang.sh" ]; then
  cp "$UPSTREAM_ENVOY_SRCDIR/bazel/setup_clang.sh" bazel/
fi

if [ -n "$COMMIT_SHA" ]; then
  echo "$COMMIT_SHA" > SOURCE_VERSION
fi

export ENVOY_SRCDIR=$SOURCE_DIR

# google cloud build doesn't like ipv6
export BAZEL_EXTRA_TEST_OPTIONS="--test_env=ENVOY_IP_TEST_VERSIONS=v4only --test_output=errors"

# We do not need/want to build the Envoy contrib filters so we replace the
# associated targets with the ENVOY_BUILD values
export ENVOY_CONTRIB_BUILD_TARGET="//source/exe:envoy-static"
export ENVOY_CONTRIB_BUILD_DEBUG_INFORMATION="//source/exe:envoy-static.dwp"

BAZEL_BUILD_EXTRA_OPTIONS+=" --remote_cache=${BAZEL_REMOTE_CACHE}"

export  GCP_SERVICE_ACCOUNT_KEY_PATH=$(mktemp -t gcp_service_account.XXXXXX.json)
echo "${GCP_SERVICE_ACCOUNT_KEY}" | base64 --decode > "${GCP_SERVICE_ACCOUNT_KEY_PATH}"
BAZEL_BUILD_EXTRA_OPTIONS+=" --google_credentials=${GCP_SERVICE_ACCOUNT_KEY_PATH}"

if [ "${BUILD_TYPE:-}" != "" ] ; then
  BUILD_CONFIG="--config=$BUILD_TYPE"
fi
echo "BUILD_CONFIG is ${BUILD_CONFIG}"

echo "test $BUILD_CONFIG" >> "${SOURCE_DIR}/test.bazelrc"

echo Building
bash -x "$UPSTREAM_ENVOY_SRCDIR/ci/do_ci.sh" "$@"

echo Extracting release binaries
ENVOY_GLOO_BIN_DIR='linux/amd64/build_envoy_release'
mkdir -p "$ENVOY_GLOO_BIN_DIR"
bazel run @envoy//tools/zstd:zstd -- --stdout -d /build/envoy/x64/bin/release.tar.zst \
    | tar xfO - envoy > "$ENVOY_GLOO_BIN_DIR/envoy"

chmod +x "${ENVOY_GLOO_BIN_DIR}/envoy"
ENVOY_GLOO_STRIPPED_DIR="${ENVOY_GLOO_BIN_DIR}_stripped"
mkdir -p "$ENVOY_GLOO_STRIPPED_DIR"
cp "${ENVOY_GLOO_BIN_DIR}/envoy" "${ENVOY_GLOO_STRIPPED_DIR}/envoy"

echo "CI completed"
