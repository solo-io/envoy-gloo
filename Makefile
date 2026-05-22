VERSION ?= $(COMMIT_SHA)
RELEASE := "false"
ifneq ($(TAGGED_VERSION),)
        VERSION := $(shell echo $(TAGGED_VERSION) | cut -c 2-)
        RELEASE := "true"
endif

ifeq ($(REGISTRY),) # Set quay.io/solo-io as default if REGISTRY is unset
        REGISTRY := quay.io/solo-io
endif

.PHONY: docker-release
docker-release:
	cd ci && docker build -t $(REGISTRY)/envoy-gloo:$(VERSION) . && docker push $(REGISTRY)/envoy-gloo:$(VERSION)

#----------------------------------------------------------------------------------
# ARM64 Builds
#----------------------------------------------------------------------------------

.PHONY: fast-build-arm
fast-build-arm:
	./ci/run_envoy_docker.sh 'ci/do_ci.sh bazel.release'

.PHONY: build-arm
build-arm:
	./ci/run_envoy_docker.sh 'ci/do_ci.sh bazel.release //test/extensions/... //test/common/... //test/integration/...'

.PHONY: docker-release-arm
docker-release-arm:
	cd ci && docker build -f Dockerfile-arm -t $(REGISTRY)/envoy-gloo-arm:$(VERSION) . && docker push $(REGISTRY)/envoy-gloo-arm:$(VERSION)

gengo:
	./ci/gen_go.sh
	cd go; go mod tidy
	cd go; go build ./...

check-gencode:
	touch SOURCE_VERSION
	CHECK=1 ./ci/gen_go.sh
	rm SOURCE_VERSION
