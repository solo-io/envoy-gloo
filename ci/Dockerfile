FROM ubuntu:16.04

RUN apt-get update \
    && apt-get upgrade -y \
    && apt-get install -y ca-certificates \
    && apt-get autoremove -y \
    && apt-get clean \
    && rm -rf /tmp/* /var/tmp/* \
    && rm -rf /var/lib/apt/lists/*

ADD envoy.stripped /usr/local/bin/envoy

ENTRYPOINT ["/usr/local/bin/envoy"]
CMD ["--v2-config-only", "-c", "/etc/envoy/envoy.yaml"]
