VERSION := "dev"
RELEASE := "false"
ifneq ($(TAGGED_VERSION),)
        VERSION := $(shell echo $(TAGGED_VERSION) | cut -c 2-)
        RELEASE := "true"
endif

.PHONY: docker-release
docker-release:
ifeq ($(RELEASE),"true")
	cd ci && docker build -t soloio/envoy-gloo:$(VERSION) . && docker push soloio/envoy-gloo:$(VERSION)
endif
