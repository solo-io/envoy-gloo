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
export USER=bazel
export TEST_TMPDIR=${BUILD_DIR}/tmp
export BAZEL="bazel"

export PPROF_PATH=/thirdparty_build/bin/pprof
[ -z "${NUM_CPUS}" ] && NUM_CPUS=`grep -c ^processor /proc/cpuinfo`

# Create a fake home. Python site libs tries to do getpwuid(3) if we don't and the CI
# Docker image gets confused as it has no passwd entry when running non-root
# unless we do this.
FAKE_HOME=/tmp/fake_home
mkdir -p "${FAKE_HOME}"
export HOME="${FAKE_HOME}"
export PYTHONUSERBASE="${FAKE_HOME}"

if [[ -f "/etc/redhat-release" ]]
then
  export BAZEL_BUILD_EXTRA_OPTIONS="--copt=-DENVOY_IGNORE_GLIBCXX_USE_CXX11_ABI_ERROR=1 --action_env=PATH ${BAZEL_BUILD_EXTRA_OPTIONS}"
else
  export BAZEL_BUILD_EXTRA_OPTIONS="--action_env=PATH=/bin:/usr/bin:/usr/lib/llvm-8/bin --linkopt=-fuse-ld=lld ${BAZEL_BUILD_EXTRA_OPTIONS}"
fi

# added by yuval-k for the integration tests to run on google cloud build
export BAZEL_EXTRA_TEST_OPTIONS="--test_env=ENVOY_IP_TEST_VERSIONS=v4only --test_output=errors"

export BAZEL_QUERY_OPTIONS="${BAZEL_OPTIONS}"
export BAZEL_BUILD_OPTIONS="--verbose_failures ${BAZEL_OPTIONS} --action_env=HOME --action_env=PYTHONUSERBASE \
  --local_cpu_resources=${NUM_CPUS} --show_task_finish --experimental_generate_json_trace_profile \
  --test_env=HOME --test_env=PYTHONUSERBASE --cache_test_results=no --test_output=all \
  ${BAZEL_BUILD_EXTRA_OPTIONS} ${BAZEL_EXTRA_TEST_OPTIONS}"

function setup_gcc_toolchain() {
  if [[ -z "${ENVOY_RBE}" ]]; then
    export CC=gcc
    export CXX=g++
    export BAZEL_COMPILER=gcc
    echo "$CC/$CXX toolchain configured"
  else
    export BAZEL_BUILD_OPTIONS="--config=rbe-toolchain-gcc ${BAZEL_BUILD_OPTIONS}"
  fi
}

function setup_clang_toolchain() {
  if [[ -z "${ENVOY_RBE}" ]]; then
    export PATH=/usr/lib/llvm-8/bin:$PATH
    export CC=clang
    export CXX=clang++
    export BAZEL_COMPILER=clang
    export ASAN_SYMBOLIZER_PATH=/usr/lib/llvm-8/bin/llvm-symbolizer
    echo "$CC/$CXX toolchain configured"
  else
    export BAZEL_BUILD_OPTIONS="--config=rbe-toolchain-clang ${BAZEL_BUILD_OPTIONS}"
  fi
}

function setup_clang_libcxx_toolchain() {
  if [[ -z "${ENVOY_RBE}" ]]; then
    export PATH=/usr/lib/llvm-8/bin:$PATH
    export CC=clang
    export CXX=clang++
    export BAZEL_COMPILER=clang
    export ASAN_SYMBOLIZER_PATH=/usr/lib/llvm-8/bin/llvm-symbolizer
    export BAZEL_BUILD_OPTIONS="--config=libc++ ${BAZEL_BUILD_OPTIONS}"
    echo "$CC/$CXX toolchain with libc++ configured"
  else
    export BAZEL_BUILD_OPTIONS="--config=rbe-toolchain-clang-libc++ ${BAZEL_BUILD_OPTIONS}"
  fi
}

function cleanup() {
  # Remove build artifacts. This doesn't mess with incremental builds as these
  # are just symlinks.
  rm -rf "${ENVOY_SRCDIR}"/bazel-*
  rm -rf "${BUILD_DIR}"/bazel-*
}

cleanup
trap cleanup EXIT

set -e

# try compiling with clang as google cloud doesn't seem to like gcc.
setup_clang_toolchain

case "$1" in
"coverage")
# TODO: test this...
    ${ENVOY_SRCDIR}test/run_envoy_bazel_coverage.sh
    ;;
"test")
    bazel test ${BAZEL_BUILD_OPTIONS} -c opt //test/...
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

