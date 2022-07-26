# This file was inspired by envoy Dockerfile:
# https://github.com/envoyproxy/envoy/blob/445a67344ffda0c8828c8e438e463fcaa7878434/ci/Dockerfile-envoy-alpine

FROM frolvlad/alpine-glibc@sha256:7b5e8e727246ea48bee1690e30f3fe35925ed9e437f87206870b2ba54fef833f

ENV loglevel=info

RUN apk upgrade --update-cache
RUN apk add dumb-init ca-certificates
RUN rm -rf /var/cache/apk/*

# Due to https://github.com/solo-io/gloo/issues/5980 we are pinned to the above alpine-glibc base image
# To prevent CVEs, we must manually specify the appropriate dependencies
# A preferrable long term solution is to be able to update the base image which should
# pull in these fixes directly
RUN apk add --update-cache --upgrade --no-cache 'libcrypto1.1>=1.1.1q-r0'
RUN apk add --update-cache --upgrade --no-cache 'libssl1.1>=1.1.1q-r0 '

RUN mkdir -p /etc/envoy

ADD envoy.stripped /usr/local/bin/envoy

ENTRYPOINT ["/usr/bin/dumb-init", "--", "/usr/local/bin/envoy"]
CMD ["-c", "/etc/envoy/envoy.yaml"]
