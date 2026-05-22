# Envoy Gloo — End of Life

Envoy Gloo is now end of life and will no longer receive releases, bug fixes, or security patches.

Envoy Gloo originally existed as the downstream Envoy distribution for [Gloo OSS](https://github.com/solo-io/gloo). As the project evolved into [kgateway](https://kgateway.dev), we moved closer to upstream open source standards. This included adopting the Kubernetes Gateway API and, starting in kgateway 2.3.0, transitioning to vanilla upstream Envoy images. kgateway 2.2.x was the final release line to use Envoy Gloo.

This shift removes the need for a custom downstream Envoy build and aligns kgateway more closely with the broader open source ecosystem. Because of this, Envoy Gloo will no longer be maintained.

If you're still running Gloo OSS, or otherwise depending on Envoy Gloo, we encourage you to migrate to [kgateway 2.3](https://github.com/kgateway-dev/kgateway/releases/tag/v2.3.0) or later: https://kgateway.dev

Thanks to everyone who contributed to Envoy Gloo over the years.

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

# ARM Support (Experimental)
Support to target arm64 architectures, such as the Raspberry Pi 2 (v1.2) 3 & 4, is **HIGLY EXPERIMENTAL**
These builds need to be executed on a machine with the same target architecture to produce the desired outcome.
(**WARNING**: If building on Raspberry Pi 4 or other equivalent SBC, using the 8GB version is recommended. To produce gloo-envoy builds, 4GB might not be enough, especially when including tests)

## Fastbuild
This method is intended to produce a gloo-envoy binary without performing tests (Might take approx. 12h to complete on a Raspberry Pi)
```bash
make fast-build-arm
```

## Build release version
Produces and tests a gloo-envoy build.
```bash
make build-arm
```

## Docker Release for ARM
You can optionally set your own registry to push gloo-envoy images by setting the REGISTRY environment variable or passing it as input. It defaults to `quay.io/solo-io`.
```bash
make docker-release-arm
```
