#!/bin/bash
#

set -e

# # create function if doesnt exist
# aws lambda create-function --function-name captialize --runtime nodejs 
# invoke
# aws lambda invoke --function-name uppercase --payload '"abc"' /dev/stdout


# prepare envoy config file.

cat > envoy.yaml << EOF
admin:
  access_log_path: /dev/stdout
  address:
    socket_address:
      address: 127.0.0.1
      port_value: 19000
static_resources:
  listeners:
  - name: listener_0
    address:
      socket_address: { address: 127.0.0.1, port_value: 10000 }
    filter_chains:
    - filters:
      - name: envoy.http_connection_manager
        config:
          stat_prefix: http
          codec_type: AUTO
          route_config:
            name: local_route
            virtual_hosts:
            - name: local_service
              domains: ["*"]
              routes:
              - match:
                  prefix: /echo
                route:
                  cluster: postman-echo
                  prefix_rewrite: /post
              - match:
                  prefix: /lambda
                route:
                  cluster: aws-us-east-1-lambda
                per_filter_config:
                  io.solo.aws_lambda:
                    name: uppercase
                    qualifier: "1"
              - match:
                  prefix: /latestlambda
                route:
                  cluster: aws-us-east-1-lambda
                per_filter_config:
                  io.solo.aws_lambda:
                    name: uppercase
                    qualifier: "%24LATEST"
              - match:
                  prefix: /contact-empty-default
                route:
                  cluster: aws-us-east-1-lambda
                per_filter_config:
                  io.solo.aws_lambda:
                    name: uppercase
                    qualifier: "1"
                    empty_body_override: "\"default-body\""
              - match:
                  prefix: /contact
                route:
                  cluster: aws-us-east-1-lambda
                per_filter_config:
                  io.solo.aws_lambda:
                    name: contact-form
                    qualifier: "3"
          http_filters:
          - name: io.solo.aws_lambda
          - name: envoy.router
  clusters:
  - connect_timeout: 5.000s
    hosts:
    - socket_address:
        address: postman-echo.com
        port_value: 443
    name: postman-echo
    type: LOGICAL_DNS
    tls_context: {}
  - connect_timeout: 5.000s
    hosts:
    - socket_address:
        address: lambda.us-east-1.amazonaws.com
        port_value: 443
    name: aws-us-east-1-lambda
    type: LOGICAL_DNS
    dns_lookup_family: V4_ONLY
    tls_context: {}
    extension_protocol_options:
      io.solo.aws_lambda:
        host: lambda.us-east-1.amazonaws.com
        region: us-east-1
        access_key: $(grep aws_access_key_id   ~/.aws/credentials | head -1 | cut -d= -f2)
        secret_key: $(grep aws_secret_access_key  ~/.aws/credentials | head -1 | cut -d= -f2)
EOF
