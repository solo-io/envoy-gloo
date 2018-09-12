# Build

## Laptop
```
bazel build -c dbg :envoy --jobs=$[$(nproc --all)-2]
```
In the command above `jobs` is set to number of CPUs minus 2. This should help keep your laptop alive.

## Optimized with debug symbols
```
bazel build -c opt :envoy
objcopy --only-keep-debug bazel-bin/envoy bazel-bin/envoy.debug
strip -g bazel-bin/envoy -o bazel-bin/envoy.striped
```

## Check stamp
Builds are stampped with the commit hash. to see the stamp:
```
eu-readelf -n bazel-bin/envoy
...
  GNU                   20  GNU_BUILD_ID
    Build ID: 45c2b7a08b480aae64cf8bb1360aea9bc0cd0576
...
```
# Test
```
bazel test -c dbg //test/... --jobs=$[$(nproc --all)-2]
```
# Test and e2e
```
bazel test -c dbg //test/... //e2e/... --jobs=$[$(nproc --all)-2]
```