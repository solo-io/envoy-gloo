# Inspired by https://github.com/envoyproxy/envoy/blob/25684687e64d47dd0c865b88d37432141770e5c7/ci/Dockerfile-envoy
# Last updated with 1.26.3
# CHECK_ON_MINOR_UPDATE do not take upstream as is, cut down to only builds we need

ARG BUILD_OS=ubuntu
ARG BUILD_TAG=20.04


FROM scratch AS binary
# this matters for envoy-gloo but is overwritten in gloo
# so we mimick this file between the repos for now
COPY ci/docker-entrypoint.sh /


# See https://github.com/docker/buildx/issues/510 for why this _must_ be this way
ARG TARGETPLATFORM
ENV TARGETPLATFORM="${TARGETPLATFORM:-linux/amd64}"
# ADD "${TARGETPLATFORM}/release.tar.zst" /usr/local/bin/
ADD envoy.stripped /usr/local/bin/envoy


# STAGE: envoy-base
FROM ${BUILD_OS}:${BUILD_TAG} AS envoy-base
ENV DEBIAN_FRONTEND=noninteractive
EXPOSE 10000
CMD ["envoy", "-c", "/etc/envoy/envoy.yaml"]
RUN mkdir -p /etc/envoy 
#   && adduser --group --system envoy # for now we can hardcode like we did previously
ENTRYPOINT ["/usr/bin/dumb-init", "--", "/docker-entrypoint.sh"]
# NB: Adding this here means that following steps, for example updating the system packages, are run
#   when the version file changes. This should mean that a release version will always update.
#   In PRs this will just use cached layers unless either this file changes or the version has changed.
ADD VERSION.txt /etc/envoy
RUN --mount=type=tmpfs,target=/var/cache/apt \
    --mount=type=tmpfs,target=/var/lib/apt/lists \
    apt-get -qq update \
    && apt-get -qq upgrade -y \
    && apt-get -qq install --no-install-recommends -y ca-certificates \
    && apt-get -qq autoremove -y


# STAGE: envoy
FROM envoy-base AS envoy
COPY --from=binary --chown=0:0 --chmod=644 \
    /etc/envoy/envoy.yaml /etc/envoy/envoy.yaml
COPY --from=binary --chown=0:0 --chmod=755 \
    /docker-entrypoint.sh /
COPY --from=binary --chown=0:0 --chmod=755 \
    /usr/local/bin/utils/su-exec /usr/local/bin/
ARG ENVOY_BINARY=envoy
ARG ENVOY_BINARY_PREFIX=
COPY --from=binary --chown=0:0 --chmod=755 \
    "/usr/local/bin/${ENVOY_BINARY_PREFIX}${ENVOY_BINARY}" /usr/local/bin/envoy
COPY --from=binary --chown=0:0 --chmod=755 \
    /usr/local/bin/${ENVOY_BINARY_PREFIX}${ENVOY_BINARY}\.* /usr/local/bin/


# TODO: Do we need this stage (probably not)

# STAGE: envoy-distroless
# gcr.io/distroless/base-nossl-debian11:nonroot
FROM gcr.io/distroless/base-nossl-debian11:nonroot@sha256:f10e1fbf558c630a4b74a987e6c754d45bf59f9ddcefce090f6b111925996767 AS envoy-distroless
EXPOSE 10000
ENTRYPOINT ["/usr/local/bin/envoy"]
CMD ["-c", "/etc/envoy/envoy.yaml"]
COPY --from=binary --chown=0:0 --chmod=644 \
    /etc/envoy/envoy.yaml /etc/envoy/envoy.yaml
COPY --from=binary --chown=0:0 --chmod=755 \
    /usr/local/bin/envoy /usr/local/bin/


# Make envoy image as last stage so it is built by default
FROM envoy
