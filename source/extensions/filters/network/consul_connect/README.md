# Envoy Filter for Consul Connect

This project links a Consul Connect filter with the Envoy binary.
A new network filter `io.solo.filters.network.consul_connect` is introduced.
The filter performs TLS client authentication against the Authorize endpoint via REST API.

The Authorize endpoint tests whether a connection attempt is authorized between two services.
Consul’s implementation of this API uses locally cached data and doesn't require any request forwarding to a server. Therefore, the response typically occurs in microseconds, to impose minimal overhead on the connection attempt.

The filter provides the presented client certificate information to the Authorize endpoint in order to determine whether the connection should be allowed or not.

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

To run all tests using a clang build:

```
$ CXX=clang++-5.0 CC=clang-5.0 bazel test -c dbg --config=clang-tsan //test/...
```

## E2E

The e2e test requires a Consul binary, which should be deployed in the following path:

```
./e2e/extensions/filters/network/consul_connect/consul
```

It also requires the [Requests](https://pypi.org/project/requests/) Python package.

To install Requests:

```
$ pip install requests
```

In addition, a server-side cetificate and the private key associated with it should be generated and deployed in the following paths:
```
/tmp/pki/root/certs/www.acme.com.crt
/tmp/pki/root/keys/www.acme.com.key
```

To run the e2e test:

```
$ bazel test //e2e/...
```
