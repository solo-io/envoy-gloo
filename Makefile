VERSION := $(COMMIT_SHA)
RELEASE := "false"
ifneq ($(TAGGED_VERSION),)
        VERSION := $(shell echo $(TAGGED_VERSION) | cut -c 2-)
        RELEASE := "true"
endif

.PHONY: docker-release
docker-release:
# ifeq ($(RELEASE),"true")
	cd ci && docker build -t quay.io/solo-io/envoy-gloo:$(VERSION) . && docker push quay.io/solo-io/envoy-gloo:$(VERSION)
# endif
