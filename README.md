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

The e2e test requires the [Requests](https://pypi.org/project/requests/) Python package and the [gRPC Python package](https://grpc.io/docs/quickstart/python.html).

To install Requests:

```
$ pip install requests
```

To install the gRPC Python package:

```
$ python -m pip install grpcio
```

It also requires [installing Python's gRPC tools](https://grpc.io/docs/quickstart/python.html#install-grpc-tools), which can be done as follows:

```
$ python -m pip install grpcio-tools googleapis-common-protos
```

To run the e2e test:

```
$ bazel test //e2e/...
```
