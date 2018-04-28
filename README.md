# Envoy cache filter

A new filter `io.solo.cache` is introduced.

## Building

To build the Envoy static binary:

```
$ bazel build //:envoy
```

## Testing

To run the all tests:

```
$ bazel test //test/...
```

To run the all tests in debug mode:

```
$ bazel test //test/... -c dbg
```

To run integration tests using a clang build:

```
$ CXX=clang++-5.0 CC=clang-5.0 bazel test -c dbg --config=clang-tsan //test/integration:cache_filter_integration_test
```

## E2E

To run the e2e test:

```
$ bazel test //e2e/...
```
