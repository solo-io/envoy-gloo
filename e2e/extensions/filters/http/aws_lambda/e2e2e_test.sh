#!/bin/bash
#

set -e

# # create function if doesnt exist
# aws lambda create-function --function-name captialize --runtime nodejs 
# invoke
# aws lambda invoke --function-name uppercase --payload '"abc"' /dev/stdout

./e2e/extensions/filters/http/aws_lambda/create_config.sh

ENVOY=${ENVOY:-envoy}

$ENVOY -c ./envoy.yaml --log-level debug & 
sleep 5


# test no impact on non lambda stuff
curl localhost:10000/echo --data '"abc"' --request POST -H"content-type: application/json"|grep abc
# Test AWS Lambda using POST method.
curl localhost:10000/lambda --data '"abc"' --request POST -H"content-type: application/json"|grep ABC

# Test AWS Lambda using GET method.
curl localhost:10000/contact |grep '<form method='

echo PASS
