#!/bin/bash
set -e
bazel fetch //source/exe:envoy-static

SOURCE_DIR="$(bazel info workspace)"


ENVOY_DEPENDENCY=$(awk '/envoy = dict/{p=1}; /commit/{print $3; exit}' bazel/repository_locations.bzl | sed -E 's|"([a-z0-9]+)",|\1|g')
echo $ENVOY_DEPENDENCY

# # will be reverted or updated in https://github.com/solo-io/envoy-gloo/issues/246
# git clone https://github.com/envoyproxy/envoy.git /tmp/envoy
# pushd /tmp/envoy
# git remote add public https://github.com/solo-io/envoy-fork
# git fetch --all
# git checkout "$ENVOY_DEPENDENCY"
# popd

$SOURCE_DIR/ci/verify_posture.sh verify

# export UPSTREAM_ENVOY_SRCDIR=/tmp/envoy
export UPSTREAM_ENVOY_SRCDIR=$(bazel info output_base)/external/envoy
cp -f $UPSTREAM_ENVOY_SRCDIR/.bazelrc $SOURCE_DIR/
cp -f $UPSTREAM_ENVOY_SRCDIR/.bazelversion $SOURCE_DIR/.bazelversion
cp -f $UPSTREAM_ENVOY_SRCDIR/ci/WORKSPACE.filter.example $SOURCE_DIR/ci/
cp -f $UPSTREAM_ENVOY_SRCDIR/VERSION.txt $SOURCE_DIR/VERSION.txt

cp -f $UPSTREAM_ENVOY_SRCDIR/tools/shell_utils.sh $SOURCE_DIR/tools


if [ -f $UPSTREAM_ENVOY_SRCDIR/bazel/setup_clang.sh ]; then
  cp $UPSTREAM_ENVOY_SRCDIR/bazel/setup_clang.sh bazel/
fi

if [ -n "$COMMIT_SHA" ]; then
  echo $COMMIT_SHA > SOURCE_VERSION
fi

export ENVOY_SRCDIR=$SOURCE_DIR

# google cloud build times out when using full throttle.
export NUM_CPUS=10

# google cloud build doesn't like ipv6
export BAZEL_EXTRA_TEST_OPTIONS="--test_env=ENVOY_IP_TEST_VERSIONS=v4only --test_output=errors --jobs=${NUM_CPUS}"

# We do not need/want to build the Envoy contrib filters so we replace the
# associated targets with the ENVOY_BUILD values
export ENVOY_CONTRIB_BUILD_TARGET="//source/exe:envoy-static"
export ENVOY_CONTRIB_BUILD_DEBUG_INFORMATION="//source/exe:envoy-static.dwp"

if [ "${BUILD_TYPE:-}" != "" ] ; then
  BUILD_CONFIG="--config=$BUILD_TYPE"
fi
echo "BUILD_CONFIG is ${BUILD_CONFIG}"

echo "test $BUILD_CONFIG" >> "${SOURCE_DIR}/test.bazelrc"

echo Building
sed -i 's|"//contrib/..."||' "$UPSTREAM_ENVOY_SRCDIR/ci/do_ci.sh"
# sed -i 's|//distribution|@envoy//distribution|' "$UPSTREAM_ENVOY_SRCDIR/ci/do_ci.sh"
# sed -i 's|bazel-bin/distribution|bazel-bin/external/envoy/distribution|' "$UPSTREAM_ENVOY_SRCDIR/ci/do_ci.sh"

bash -x $UPSTREAM_ENVOY_SRCDIR/ci/do_ci.sh "$@"

$SOURCE_DIR/ci/static_analysis.sh

echo "CI completed"
