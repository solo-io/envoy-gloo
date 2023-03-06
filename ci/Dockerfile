# This file was inspired by envoy Dockerfile:
# https://github.com/envoyproxy/envoy/blob/445a67344ffda0c8828c8e438e463fcaa7878434/ci/Dockerfile-envoy-alpine

FROM frolvlad/alpine-glibc@sha256:6339ac698a4c9b377a6c1c08f9ae7ab71f01e050937d519cbec352e028afaa40

ENV loglevel=info

RUN apk upgrade --update-cache \
    && apk add dumb-init ca-certificates \
    && apk add --no-cache \
    && apk add libcrypto1.1=1.1.1t-r1 \
    && apk add libssl1.1=1.1.1t-r1  \
    && rm -rf /var/cache/apk/*


RUN mkdir -p /etc/envoy

ADD envoy.stripped /usr/local/bin/envoy

ENTRYPOINT ["/usr/bin/dumb-init", "--", "/usr/local/bin/envoy"]
CMD ["-c", "/etc/envoy/envoy.yaml"]
