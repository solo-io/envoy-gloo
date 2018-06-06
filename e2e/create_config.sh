#!/bin/bash
#

set -e

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
      - name: io.solo.client_certificate_restriction
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
                  prefix: /get
                route:
                  cluster: cluster_0
          http_filters:
          - name: envoy.router
      tls_context:
        common_tls_context:
          tls_certificates:
          - certificate_chain:
              filename: /tmp/pki/root/certs/www.acme.com.crt
            private_key:
              filename: /tmp/pki/root/keys/www.acme.com.key
          tls_params: {}
          validation_context:
            trusted_ca:
              filename: /tmp/pki/root/certs/root.crt
  clusters:
  - connect_timeout: 5.000s
    hosts:
    - socket_address:
        address: 127.0.0.1
        port_value: 4222
    name: cluster_0
    type: STRICT_DNS
EOF
