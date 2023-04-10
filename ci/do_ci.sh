#!/bin/bash
set -e
bazel fetch //source/exe:envoy-static

SOURCE_DIR="$(bazel info workspace)"

$SOURCE_DIR/ci/verify_posture.sh verify

export UPSTREAM_ENVOY_SRCDIR=$(bazel info output_base)/external/envoy
cp -f $UPSTREAM_ENVOY_SRCDIR/.bazelrc $SOURCE_DIR/
# dont think this is needed... cp -f $UPSTREAM_ENVOY_SRCDIR/*.bazelrc $SOURCE_DIR/
cp -f $UPSTREAM_ENVOY_SRCDIR/.bazelversion $SOURCE_DIR/.bazelversion
# cp -f $UPSTREAM_ENVOY_SRCDIR/bazel/get_workspace_status $SOURCE_DIR/bazel/get_workspace_status
cp -f $UPSTREAM_ENVOY_SRCDIR/ci/WORKSPACE.filter.example $SOURCE_DIR/ci/

mkdir -p $SOURCE_DIR/ci/flaky_test
cp -a $UPSTREAM_ENVOY_SRCDIR/ci/flaky_test $SOURCE_DIR/ci

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

# sudo apt-get install google-perftools -y
# export PPROF_PATH=$(which google-pprof)

if [ "x${BUILD_TYPE:-}" != "x" ] ; then
  BUILD_CONFIG="--config=$BUILD_TYPE"
fi
echo "$BUILD_CONFIG is ${BUILD_CONFIG}"

echo "test $BUILD_CONFIG" >> "${SOURCE_DIR}/.bazelrc"

echo Building
sed -i "s|//test/tools/schema_validator:schema_validator_tool|@envoy//test/tools/schema_validator:schema_validator_tool|" "$UPSTREAM_ENVOY_SRCDIR/ci/do_ci.sh"
sed -i "s|bazel-bin/test/tools/schema_validator/schema_validator_tool|bazel-bin/external/envoy/test/tools/schema_validator/schema_validator_tool|" "$UPSTREAM_ENVOY_SRCDIR/ci/do_ci.sh"
sed -i "s|VERSION.txt|ci/FAKEVERSION.txt|" "$UPSTREAM_ENVOY_SRCDIR/ci/do_ci.sh"
sed -i "s|\${ENVOY_SRCDIR}/VERSION.txt|ci/FAKEVERSION.txt|" "$UPSTREAM_ENVOY_SRCDIR/ci/build_setup.sh"

bash -x $UPSTREAM_ENVOY_SRCDIR/ci/do_ci.sh "$@"

$SOURCE_DIR/ci/static_analysis.sh

echo "CI completed"
