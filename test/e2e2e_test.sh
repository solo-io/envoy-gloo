#!/bin/bash
#

set -e

# # create function if doesnt exist
# aws lambda create-function --function-name captialize --runtime nodejs 
# invoke
# aws lambda invoke --function-name uppercase --payload '"abc"' /dev/stdout

./create_config.sh || ./test/create_config.sh 

ENVOY=${ENVOY:-envoy}

$ENVOY -c ./envoy.yaml --log-level debug & 
sleep 5


# test no impact on non lambda stuff
curl localhost:10000/echo --data '"abc"' --request POST -H"content-type: application/json"|grep abc
# test lambda
curl localhost:10000/lambda --data '"abc"' --request POST -H"content-type: application/json"|grep ABC

echo PASS