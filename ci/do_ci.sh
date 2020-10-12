#!/bin/bash
set -e
bazel fetch //source/exe:envoy-static

SOURCE_DIR="$(bazel info workspace)"


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

# sudo apt-get install google-perftools -y
# export PPROF_PATH=$(which google-pprof) 

echo Building
bash -x $UPSTREAM_ENVOY_SRCDIR/ci/do_ci.sh "$@"
