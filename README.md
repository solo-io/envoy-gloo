# Envoy Lambda filter

This project links an AWS Lambda HTTP filter with the Envoy binary.
A new filter `io.solo.lambda` which redirects requests to AWS Lambda is introduced.

## Building

To build the Envoy static binary:

`bazel build //:envoy`

## Testing

To run the all tests:

`bazel test //test:*`

To run integration tests using a clang build:

`CXX=clang++-5.0 CC=clang-5.0  bazel test -c dbg --config=clang-tsan //test:lambda_filter_integration_test --runs_per_test=10`
