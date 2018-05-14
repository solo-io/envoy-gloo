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
          - name: io.solo.cache
            config:
              in_memory: on
              google_grpc_target_uri: "127.0.0.1:50051"
              grpc_timeout: 1s
              redis_cluster: redis
              redis_stat_prefix: stat
              redis_op_timeout: 5s
          - name: envoy.router
  clusters:
  - connect_timeout: 5.000s
    hosts:
    - socket_address:
        address: 127.0.0.1
        port_value: 4222
    name: cluster_0
    type: STRICT_DNS
  - connect_timeout: 5.000s
    hosts:
    - socket_address:
        address: 127.0.0.1
        port_value: 6379
    name: redis
    type: STRICT_DNS
  - connect_timeout: 5.000s
    hosts:
    - socket_address:
        address: 127.0.0.1
        port_value: 50051
    name: grpc
    type: STRICT_DNS
EOF
