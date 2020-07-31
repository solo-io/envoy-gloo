#!/bin/bash
#

set -e

# # create function if doesnt exist
# aws lambda create-function --function-name captialize --runtime nodejs 
# invoke
# aws lambda invoke --function-name uppercase --payload '"abc"' /dev/stdout

./e2e/extensions/filters/http/aws_lambda/create_config.sh

ENVOY=${ENVOY:-envoy}

echo $ENVOY
pwd

$ENVOY --concurrency 2 --disable-hot-restart -c ./envoy.yaml --log-level trace & 
sleep 5


# test no impact on non lambda stuff
curl localhost:10000/echo --data '"abc"' --request POST -H"content-type: application/json"|grep abc
# Test AWS Lambda using POST method.
curl localhost:10000/lambda --data '"abc"' --request POST -H"content-type: application/json"|grep ABC

# Test AWS Lambda using GET method.
curl localhost:10000/contact |grep '<form method='

# Test AWS Lambda using GET method with empty body that turns into non empty.
curl localhost:10000/contact-empty-default |grep 'DEFAULT-BODY'

curl localhost:19000/quitquitquit -XPOST


####################### part 2 with env

# Sanity with env:
echo
echo testing with env credentials
echo

. ./e2e/extensions/filters/http/aws_lambda/create_config_env.sh
$ENVOY --concurrency 2 --disable-hot-restart -c ./envoy_env.yaml --log-level trace & 
sleep 5

curl localhost:10001/lambda --data '"abc"' --request POST -H"content-type: application/json"|grep ABC

curl localhost:19000/quitquitquit -XPOST


####################### part 3 with env + token

# Sanity with env:
echo
echo testing with env credentials + token
echo

. ./e2e/extensions/filters/http/aws_lambda/create_config_env_token.sh
$ENVOY --concurrency 2 --disable-hot-restart -c ./envoy_env.yaml --log-level trace & 
sleep 5

curl localhost:10001/lambda --data '"abc"' --request POST -H"content-type: application/json"|grep ABC

curl localhost:19000/quitquitquit -XPOST

echo PASS


####################### part 4 with STS

# Sanity with web token:
echo
echo testing with STS credentials
echo

. ./e2e/extensions/filters/http/aws_lambda/create_config_web_token.sh
$ENVOY --concurrency 2 --disable-hot-restart -c ./envoy_env.yaml --log-level trace & 
sleep 10

curl localhost:10001/lambda --data '"abc"' --request POST -H"content-type: application/json"|grep ABC

curl localhost:19000/quitquitquit -XPOST

echo PASS

