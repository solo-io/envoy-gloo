# Lambda

## Overview

Lambda is an HTTP filter which enables Envoy to integrate with
[AWS Lambda](https://aws.amazon.com/lambda/).

Once a request routed for AWS Lambda enters the service mesh, the Lambda Envoy filter uses the
configured AWS credentials to authenticate this request. It then forwards the request to the
designated AWS Lambda function in the appropriate AWS region.

## Configuration

* [v2 API reference](../../api-v2/config/filter/http/http.md)

## How it works

When the Lambda filter encounters a request routed to an AWS Lambda upstream it will:

1. Set the request method to POST.
1. Generate a URI using the Lambda function name, and use this URI as the request's destination
path. You can use an optional Qualifier to specify a Lambda function version or alias name.
1. Populate the proprietary AWS Lambda headers using the function's invocation type.
This allows optionally requesting asynchronous execution.
1. Populate the request's host using the AWS host name associated with the specified region.
1. Sign the request using the configured AWS credentials.
