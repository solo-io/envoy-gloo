#!/bin/bash
#

set -e

# # create function if doesnt exist
# aws lambda create-function --function-name captialize --runtime nodejs 
# invoke
# aws lambda invoke --function-name uppercase --payload '"abc"' /dev/stdout


# prepare envoy config file.

cat > envoy_env.yaml << EOF
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
      socket_address: { address: 127.0.0.1, port_value: 10001 }
    filter_chains:
    - filters:
      - name: envoy.filters.network.http_connection_manager
        typed_config:
          "@type": type.googleapis.com/envoy.extensions.filters.network.http_connection_manager.v3.HttpConnectionManager
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
                typed_per_filter_config:
                  io.solo.aws_lambda:
                    "@type": type.googleapis.com/envoy.config.filter.http.aws_lambda.v2.AWSLambdaPerRoute
                    name: uppercase
                    qualifier: "1"
              - match:
                  prefix: /latestlambda
                route:
                  cluster: aws-us-east-1-lambda
                typed_per_filter_config:
                  io.solo.aws_lambda:
                    "@type": type.googleapis.com/envoy.config.filter.http.aws_lambda.v2.AWSLambdaPerRoute
                    name: uppercase
                    qualifier: "%24LATEST"
              - match:
                  prefix: /contact-empty-default
                route:
                  cluster: aws-us-east-1-lambda
                typed_per_filter_config:
                  io.solo.aws_lambda:
                    "@type": type.googleapis.com/envoy.config.filter.http.aws_lambda.v2.AWSLambdaPerRoute
                    name: uppercase
                    qualifier: "1"
                    empty_body_override: "\"default-body\""
              - match:
                  prefix: /contact
                route:
                  cluster: aws-us-east-1-lambda
                typed_per_filter_config:
                  io.solo.aws_lambda:
                    "@type": type.googleapis.com/envoy.config.filter.http.aws_lambda.v2.AWSLambdaPerRoute
                    name: contact-form
                    qualifier: "3"
          http_filters:
          - name: io.solo.aws_lambda
            typed_config:
              "@type": type.googleapis.com/envoy.config.filter.http.aws_lambda.v2.AWSLambdaConfig
              service_account_credentials:
                cluster: aws-sts
                uri: sts.amazonaws.com
          - name: envoy.filters.http.router
  clusters:
    - name: postman-echo
      type: LOGICAL_DNS
      # Comment out the following line to test on v6 networks
      dns_lookup_family: V4_ONLY
      load_assignment:
        cluster_name: postman-echo
        endpoints:
        - lb_endpoints:
          - endpoint:
              address:
                socket_address:
                  address: postman-echo.com
                  port_value: 443
      transport_socket:
        name: envoy.transport_sockets.tls
        # typed_config:
        #   "@type": type.googleapis.com/envoy.extensions.transport_sockets.tls.v3.UpstreamTlsContext
        #   sni: www.postman-echo.com
    - name: aws-sts
      type: LOGICAL_DNS
      # Comment out the following line to test on v6 networks
      dns_lookup_family: V4_ONLY
      load_assignment:
        cluster_name: aws-sts
        endpoints:
        - lb_endpoints:
          - endpoint:
              address:
                socket_address:
                  address: sts.amazonaws.com
                  port_value: 443
      transport_socket:
        name: envoy.transport_sockets.tls
        typed_config:
          "@type": type.googleapis.com/envoy.extensions.transport_sockets.tls.v3.UpstreamTlsContext
          sni: sts.amazonaws.com
    - name: aws-us-east-1-lambda
      type: LOGICAL_DNS
      dns_lookup_family: V4_ONLY
      typed_extension_protocol_options:
        io.solo.aws_lambda:
          "@type": type.googleapis.com/envoy.config.filter.http.aws_lambda.v2.AWSLambdaProtocolExtension
          host: lambda.us-east-1.amazonaws.com
          region: us-east-1
      transport_socket:
        name: envoy.transport_sockets.tls
      load_assignment:
        cluster_name: aws-us-east-1-lambda
        endpoints:
        - lb_endpoints:
          - endpoint:
              address:
                socket_address:
                  address: lambda.us-east-1.amazonaws.com
                  port_value: 443
EOF

TEMP_FILE=$(mktemp)

echo $AWS_WEB_TOKEN > $TEMP_FILE


export AWS_WEB_IDENTITY_TOKEN_FILE=$TEMP_FILE
export AWS_ROLE_ARN=$AWS_ROLE_ARN
