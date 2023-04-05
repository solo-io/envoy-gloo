# This file was inspired by envoy Dockerfile:
# https://github.com/envoyproxy/envoy/blob/445a67344ffda0c8828c8e438e463fcaa7878434/ci/Dockerfile-envoy-alpine

FROM frolvlad/alpine-3.17_glibc-2.34

ENV loglevel=info

RUN apk upgrade --update-cache \
    && apk add dumb-init ca-certificates \
    && rm -rf /var/cache/apk/*

RUN mkdir -p /etc/envoy

ADD envoy.stripped /usr/local/bin/envoy

ENTRYPOINT ["/usr/bin/dumb-init", "--", "/usr/local/bin/envoy"]
CMD ["-c", "/etc/envoy/envoy.yaml"]
