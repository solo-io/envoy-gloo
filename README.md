# Envoy Lambda filter

This project links an AWS Lambda HTTP filter with the Envoy binary.
A new filter `io.solo.lambda` which redirects requests to AWS Lambda is introduced.

## Building

To build the Envoy static binary:

`bazel build //:envoy`

## Testing

To run the all tests:

`bazel test //test:*`
