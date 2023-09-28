# This file was inspired by mesh Dockerfile:
# https://github.com/solo-io/gloo-mesh-enterprise/blob/c40b21c3ef260de1acc92ba25bd1794b17074c8c/docker/Dockerfile.ubuntu#L4
# as it previously was based off of envoys old alpine setup. 
FROM ubuntu:focal-20220826

ENV loglevel=info

ENV DEBIAN_FRONTEND=noninteractive

# hadolint ignore=DL3005,DL3008
RUN apt-get update && \
  apt-get install --no-install-recommends -y \
  ca-certificates \
  && apt-get upgrade -y \
  && apt-get clean \
  && rm -rf  /var/log/*log /var/lib/apt/lists/* /var/log/apt/* /var/lib/dpkg/*-old /var/cache/debconf/*-old

RUN mkdir -p /etc/envoy

ADD envoy.stripped /usr/local/bin/envoy

ENTRYPOINT ["/usr/local/bin/envoy"]
CMD ["-c", "/etc/envoy/envoy.yaml"]
