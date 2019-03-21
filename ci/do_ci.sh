#!/bin/bash

set -x

if [ -n "$COMMIT_SHA" ]; then
  echo $COMMIT_SHA > SOURCE_VERSION
fi

export BUILD_DIR=/build

if [[ ! -d "${BUILD_DIR}" ]]
then
  echo "${BUILD_DIR} mount missing - did you forget -v <something>:${BUILD_DIR}? Creating."
  mkdir -p "${BUILD_DIR}"
fi

ENVOY_SRCDIR=$PWD
ENVOY_CIDIR=${ENVOY_SRCDIR}/ci
# replace bazel cache to be in our volume
export TEST_TMPDIR=${BUILD_DIR}/tmp
export USER=root

BAZEL_OPTIONS="--package_path %workspace%:${ENVOY_SRCDIR}"

[ -z "${NUM_CPUS}" ] && NUM_CPUS=`grep -c ^processor /proc/cpuinfo`

# Create a fake home. Python site libs tries to do getpwuid(3) if we don't and the CI
# Docker image gets confused as it has no passwd entry when running non-root
# unless we do this.
FAKE_HOME=/tmp/fake_home
mkdir -p "${FAKE_HOME}"
export HOME="${FAKE_HOME}"
export PYTHONUSERBASE="${FAKE_HOME}"

export BAZEL_BUILD_OPTIONS="--strategy=Genrule=standalone --genrule_strategy=standalone \
  --spawn_strategy=standalone \
  --verbose_failures ${BAZEL_OPTIONS} --action_env=HOME --action_env=PYTHONUSERBASE \
  --jobs=${NUM_CPUS} --show_task_finish ${BAZEL_BUILD_EXTRA_OPTIONS}"

export BAZEL_TEST_OPTIONS="${BAZEL_BUILD_OPTIONS} --test_env=HOME --test_env=PYTHONUSERBASE \
  --test_env=UBSAN_OPTIONS=print_stacktrace=1 \
  --cache_test_results=no --test_output=all ${BAZEL_EXTRA_TEST_OPTIONS}"

function setup_gcc_toolchain() {
  export CC=gcc
  export CXX=g++
  echo "$CC/$CXX toolchain configured"
}

function setup_clang_toolchain() {
  export PATH=/usr/lib/llvm-7/bin:$PATH
  export CC=clang
  export CXX=clang++
  export ASAN_SYMBOLIZER_PATH=/usr/lib/llvm-7/bin/llvm-symbolizer
  echo "$CC/$CXX toolchain configured"
}

function cleanup() {
  # Remove build artifacts. This doesn't mess with incremental builds as these
  # are just symlinks.
  rm -rf "${ENVOY_SRCDIR}"/bazel-*
  rm -rf "${BUILD_DIR}"/bazel-*
}

cleanup
trap cleanup EXIT


# link prebuilt stuff

ln -sf /thirdparty "${ENVOY_CIDIR}"/prebuilt
ln -sf /thirdparty_build "${ENVOY_CIDIR}"/prebuilt

# Replace the existing Bazel output cache with a copy of the image's prebuilt deps.
if [[ -d /bazel-prebuilt-output && ! -d "${TEST_TMPDIR}/_bazel_${USER}" ]]; then
  BAZEL_OUTPUT_BASE="$(bazel info output_base)"
  mkdir -p "${TEST_TMPDIR}/_bazel_${USER}/install"
  rsync -a /bazel-prebuilt-root/install/* "${TEST_TMPDIR}/_bazel_${USER}/install/"
  rsync -a /bazel-prebuilt-output "${BAZEL_OUTPUT_BASE}"
fi

set -e

# try compiling with clang as google cloud doesn't seem to like gcc.
setup_clang_toolchain

case "$1" in
"coverage")
# TODO: test this...
    ${ENVOY_SRCDIR}test/run_envoy_bazel_coverage.sh
    ;;
"test")
    # build all the tests to prevent code rot.
    bazel build ${BAZEL_TEST_OPTIONS} -c opt //test/...
    echo "Tests built. running tests."
    # only run extensions for now as integration tests fail on cloud build.
    bazel test ${BAZEL_TEST_OPTIONS} -c opt //test/extensions/...
    ;;
"build")
    bazel build ${BAZEL_BUILD_OPTIONS} -c opt :envoy
    objcopy --only-keep-debug bazel-bin/envoy bazel-bin/envoy.debuginfo
    strip -g bazel-bin/envoy -o bazel-bin/envoy.stripped

    # copy binaries to the ci folder so they will be available in other containers.
    cp bazel-bin/envoy.debuginfo bazel-bin/envoy bazel-bin/envoy.stripped "${ENVOY_CIDIR}"

    ;;
*)
    exit 1
    ;;
esac

