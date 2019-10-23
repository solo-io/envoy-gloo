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

## Upload debug symbols to s3

Create bucket (only one time):
```
aws s3 mb s3://artifacts.solo.io
```

Copy:
```
BUILDID=$(readelf -n ./bazel-bin/envoy|grep "Build ID:" |cut -f2 -d:|tr -d ' ')
aws s3 cp bazel-bin/envoy.debug s3://artifacts.solo.io/$BUILDID/envoy.debug
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

## Bazel dependencies, tips
- Dependencies
  - ninja
- Troubleshooting
  - remove build artifacts: `bazel clean --expunge`
  - update bazel

# Test
```
bazel test -c dbg //test/... --jobs=$[$(nproc --all)-2]
```
# Test and e2e
```
bazel test -c dbg //test/... //e2e/... --jobs=$[$(nproc --all)-2]
```

# Format fix
```
BUILDIFIER=$GOPATH/bin/buildifier CLANG_FORMAT=clang-format /path/to/envoy/tools/check_format.py fix --skip_envoy_build_rule_check
```

# Submit a build
```
gcloud builds submit --project=solo-public --config=cloudbuild.yaml \
   --substitutions=COMMIT_SHA=$(git rev-parse HEAD) .
```
